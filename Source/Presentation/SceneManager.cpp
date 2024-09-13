// © 2022 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "BoundsComponent.h"
#include "Camera.h"
#include "CameraComponent.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshPrimitiveComponent.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "TransformComponent.h"

#include "Core/Config.h"
#include "Core/PerformanceTimer.h"
#include "Core/ThreadPool.h"

#include "Core/Util/ByteVector.h"
#include "Core/Util/CastUtils.h"
#include "Core/Util/ThreadSafeVector.h"

#include "Renderer/AssetLoadUtils.h"
#include "Renderer/Material_GLTF.h"
#include "Renderer/RenderManager.h"
#include "Renderer/VertexStreamBuilder.h"

#pragma warning(disable : 4996) // Suppress error C4996 (Caused by use of fopen, strcpy, strncpy in cgltf.h)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"


namespace
{
	constexpr size_t k_initialBatchReservations = 100;

	// Each element/index corresponds to an animation: Multiple animations may target the same node
	using AnimationNodeToDataMaps = std::vector<std::unordered_map<cgltf_node const*, fr::AnimationData>>;


	std::shared_ptr<re::Texture> LoadTextureOrColor(
		re::SceneData& scene,
		std::string const& sceneRootPath,
		cgltf_texture* texture,
		glm::vec4 const& colorFallback,
		re::Texture::Format formatFallback,
		re::Texture::ColorSpace colorSpace)
	{
		SEAssert(formatFallback != re::Texture::Format::Depth32F && formatFallback != re::Texture::Format::Invalid,
			"Invalid fallback format");

		std::shared_ptr<re::Texture> tex;
		if (texture && texture->image)
		{
			if (texture->image->uri && std::strncmp(texture->image->uri, "data:image/", 11) == 0) // uri = embedded data
			{
				// Unpack the base64 data embedded in the URI. Note: Usage of cgltf's cgltf_load_buffer_base64 function
				// is currently not well documented. This solution was cribbed from Google's filament usage
				// (parseDataUri, line 285):
				// https://github.com/google/filament/blob/676694e4589dca55c1cdbbb669cf3dba0e2b576f/libs/gltfio/src/ResourceLoader.cpp

				const char* comma = strchr(texture->image->uri, ',');
				if (comma && comma - texture->image->uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
				{
					const char* base64 = comma + 1;
					const size_t base64Size = strlen(base64);
					size_t size = base64Size - base64Size / 4;
					if (base64Size >= 2) {
						size -= base64[base64Size - 2] == '=';
						size -= base64[base64Size - 1] == '=';
					}
					void* data = 0;
					cgltf_options options = {};
					cgltf_result result = cgltf_load_buffer_base64(&options, size, base64, &data);

					// Data is decoded, now load it as usual:
					const std::string texNameStr = grutil::GenerateEmbeddedTextureName(texture->image->name);
					if (scene.TextureExists(texNameStr))
					{
						tex = scene.GetTexture(texNameStr);
					}
					else
					{
						tex = grutil::LoadTextureFromMemory(
							texNameStr, static_cast<unsigned char const*>(data), static_cast<uint32_t>(size), colorSpace);
					}
				}
			}
			else if (texture->image->uri) // uri is a filename (e.g. "myImage.png")
			{
				const std::string texNameStr = sceneRootPath + texture->image->uri;
				if (scene.TextureExists(texNameStr))
				{
					tex = scene.GetTexture(texNameStr);
				}
				else
				{
					tex = grutil::LoadTextureFromFilePath(
						{ texNameStr }, colorSpace, false, re::Texture::k_errorTextureColor);
				}
			}
			else if (texture->image->buffer_view) // texture data is already loaded in memory
			{
				const std::string texNameStr = grutil::GenerateEmbeddedTextureName(texture->image->name);

				if (scene.TextureExists(texNameStr))
				{
					tex = scene.GetTexture(texNameStr);
				}
				else
				{
					unsigned char const* texSrc = static_cast<unsigned char const*>(
						texture->image->buffer_view->buffer->data) + texture->image->buffer_view->offset;

					const uint32_t texSrcNumBytes = static_cast<uint32_t>(texture->image->buffer_view->size);
					tex = grutil::LoadTextureFromMemory(texNameStr, texSrc, texSrcNumBytes, colorSpace);
				}
			}

			SEAssert(tex != nullptr, "Failed to load texture: Does the asset exist?");
		}
		else
		{
			// Create a texture color fallback:
			re::Texture::TextureParams colorTexParams
			{
				.m_usage = 
					static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = formatFallback,
				.m_colorSpace = colorSpace
			};

			const size_t numChannels = re::Texture::GetNumberOfChannels(formatFallback);
			const std::string fallbackName = grutil::GenerateTextureColorFallbackName(colorFallback, numChannels, colorSpace);

			if (scene.TextureExists(fallbackName))
			{
				tex = scene.GetTexture(fallbackName);
			}
			else
			{
				tex = re::Texture::Create(fallbackName, colorTexParams, colorFallback);
			}
		}

		return tex;
	}


	void GenerateDefaultResources(re::SceneData& scene)
	{
		LOG("Generating default resources...");

		// Default error material:
		LOG("Generating an error material \"%s\"...", en::DefaultResourceNames::k_missingMaterialName);

		std::shared_ptr<gr::Material> errorMat = gr::Material::Create(
			en::DefaultResourceNames::k_missingMaterialName,
			gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness);

		// MatAlbedo
		std::shared_ptr<re::Texture> errorAlbedo = grutil::LoadTextureFromFilePath(
			{ en::DefaultResourceNames::k_missingAlbedoTexName },
			re::Texture::ColorSpace::sRGB,
			true,
			re::Texture::k_errorTextureColor);
		errorMat->SetTexture(0, errorAlbedo);

		// MatMetallicRoughness
		std::shared_ptr<re::Texture> errorMetallicRoughness = grutil::LoadTextureFromFilePath(
			{ en::DefaultResourceNames::k_missingMetallicRoughnessTexName },
			re::Texture::ColorSpace::Linear,
			true,
			glm::vec4(0.f, 1.f, 0.f, 0.f));
		errorMat->SetTexture(1, errorMetallicRoughness);

		// MatNormal
		std::shared_ptr<re::Texture> errorNormal = grutil::LoadTextureFromFilePath(
			{ en::DefaultResourceNames::k_missingNormalTexName },
			re::Texture::ColorSpace::Linear,
			true,
			glm::vec4(0.5f, 0.5f, 1.0f, 0.0f));
		errorMat->SetTexture(2, errorNormal);

		// MatOcclusion
		std::shared_ptr<re::Texture> errorOcclusion = grutil::LoadTextureFromFilePath(
			{ en::DefaultResourceNames::k_missingOcclusionTexName },
			re::Texture::ColorSpace::Linear,
			true,
			glm::vec4(1.f));
		errorMat->SetTexture(3, errorOcclusion);

		// MatEmissive
		std::shared_ptr<re::Texture> errorEmissive = grutil::LoadTextureFromFilePath(
			{ en::DefaultResourceNames::k_missingEmissiveTexName },
			re::Texture::ColorSpace::sRGB,
			true,
			re::Texture::k_errorTextureColor);
		errorMat->SetTexture(4, errorEmissive);

		scene.AddUniqueMaterial(errorMat);


		// Default 2D texture fallbacks:
		const re::Texture::TextureParams defaultTexParams = re::Texture::TextureParams
		{
			.m_usage = 
				static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
			.m_dimension = re::Texture::Dimension::Texture2D,
			.m_format = re::Texture::Format::RGBA8_UNORM,
			.m_colorSpace = re::Texture::ColorSpace::Linear
		};

		re::Texture::Create(
			en::DefaultResourceNames::k_opaqueWhiteDefaultTexName,
			defaultTexParams,
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		re::Texture::Create(
			en::DefaultResourceNames::k_transparentWhiteDefaultTexName,
			defaultTexParams,
			glm::vec4(1.f, 1.f, 1.f, 0.f));

		re::Texture::Create(
			en::DefaultResourceNames::k_opaqueBlackDefaultTexName,
			defaultTexParams,
			glm::vec4(0.f, 0.f, 0.f, 1.f));

		re::Texture::Create(
			en::DefaultResourceNames::k_transparentBlackDefaultTexName,
			defaultTexParams,
			glm::vec4(0.f, 0.f, 0.f, 0.f));


		// Default cube map texture fallbacks:
		const re::Texture::TextureParams defaultCubeMapTexParams = re::Texture::TextureParams
		{
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
			.m_dimension = re::Texture::Dimension::TextureCube,
			.m_format = re::Texture::Format::RGBA8_UNORM,
			.m_colorSpace = re::Texture::ColorSpace::Linear
		};

		re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapOpaqueWhiteDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapTransparentWhiteDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(1.f, 1.f, 1.f, 0.f));

		re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapOpaqueBlackDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(0.f, 0.f, 0.f, 1.f));

		re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapTransparentBlackDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(0.f, 0.f, 0.f, 0.f));
	}


	void PreLoadMaterials(std::string const& sceneRootPath, re::SceneData& scene, cgltf_data* data)
	{
		const size_t numMaterials = data->materials_count;
		LOG("Loading %d scene materials", numMaterials);

		// We assign each material to a thread; These threads will spawn new threads to load each texture. We need to 
		// wait on the future of each material to know when we can begin waiting on the futures for its textures
		std::vector<std::future<util::ThreadSafeVector<std::future<void>>>> matFutures;
		matFutures.reserve(numMaterials);

		for (size_t cur = 0; cur < numMaterials; cur++)
		{
			matFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob([data, cur, &scene, &sceneRootPath]()
				-> util::ThreadSafeVector<std::future<void>>
				{

					util::ThreadSafeVector<std::future<void>> textureFutures;
					textureFutures.reserve(5); // Albedo, met/rough, normal, occlusion, emissive

					cgltf_material const* const material = &data->materials[cur];

					const std::string matName =
						material == nullptr ? "MissingMaterial" : grutil::GenerateMaterialName(*material);
					if (scene.MaterialExists(matName))
					{
						LOG_WARNING(
							"Found materials with dupicate names. Assuming all instances of \"%s\" are identical",
							matName.c_str());

						return textureFutures;
					}

					LOG("Loading material \"%s\"", matName.c_str());

					SEAssert(material->has_pbr_metallic_roughness == 1,
						"We currently only support the PBR metallic/roughness material model");

					std::shared_ptr<gr::Material> newMat =
						gr::Material::Create(matName, gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness);

					// GLTF specifications: If a texture is not given, all texture components are assumed to be 1.f
					// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
					constexpr glm::vec4 missingTextureColor(1.f, 1.f, 1.f, 1.f);

					// MatAlbedo
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(0, LoadTextureOrColor(
								scene,
								sceneRootPath,
								material->pbr_metallic_roughness.base_color_texture.texture,
								missingTextureColor,
								re::Texture::Format::RGBA8_UNORM,
								re::Texture::ColorSpace::sRGB));
						}));

					// MatMetallicRoughness
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(1, LoadTextureOrColor(
								scene,
								sceneRootPath,
								material->pbr_metallic_roughness.metallic_roughness_texture.texture,
								missingTextureColor,
								re::Texture::Format::RGBA8_UNORM,
								re::Texture::ColorSpace::Linear));
						}));

					// MatNormal
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(2, LoadTextureOrColor(
								scene,
								sceneRootPath,
								material->normal_texture.texture,
								glm::vec4(0.5f, 0.5f, 1.0f, 0.0f), // Equivalent to a [0,0,1] normal after unpacking
								re::Texture::Format::RGBA8_UNORM,
								re::Texture::ColorSpace::Linear));
						}));

					// MatOcclusion
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(3, LoadTextureOrColor(
								scene,
								sceneRootPath,
								material->occlusion_texture.texture,
								missingTextureColor,	// Completely unoccluded
								re::Texture::Format::RGBA8_UNORM,
								re::Texture::ColorSpace::Linear));
						}));

					// MatEmissive
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(4, LoadTextureOrColor(
								scene,
								sceneRootPath,
								material->emissive_texture.texture,
								missingTextureColor,
								re::Texture::Format::RGBA8_UNORM,
								re::Texture::ColorSpace::sRGB)); // GLTF convention: Must be converted to linear before use
						}));

					gr::Material_GLTF* newGLTFMat = newMat->GetAs<gr::Material_GLTF*>();

					newGLTFMat->SetBaseColorFactor(glm::make_vec4(material->pbr_metallic_roughness.base_color_factor));
					newGLTFMat->SetMetallicFactor(material->pbr_metallic_roughness.metallic_factor);
					newGLTFMat->SetRoughnessFactor(material->pbr_metallic_roughness.roughness_factor);
					newGLTFMat->SetNormalScale(material->normal_texture.texture ? material->normal_texture.scale : 1.0f);
					newGLTFMat->SetOcclusionStrength(material->occlusion_texture.texture ? material->occlusion_texture.scale : 1.0f);

					newGLTFMat->SetEmissiveFactor(glm::make_vec3(material->emissive_factor));
					newGLTFMat->SetEmissiveStrength(
						material->has_emissive_strength ? material->emissive_strength.emissive_strength : 1.0f);

					switch (material->alpha_mode)
					{
					case cgltf_alpha_mode::cgltf_alpha_mode_opaque:
						newGLTFMat->SetAlphaMode(gr::Material::AlphaMode::Opaque); break;
					case cgltf_alpha_mode::cgltf_alpha_mode_mask:
						newGLTFMat->SetAlphaMode(gr::Material::AlphaMode::Mask); break;
					case cgltf_alpha_mode::cgltf_alpha_mode_blend:
						newGLTFMat->SetAlphaMode(gr::Material::AlphaMode::Blend); break;
					}

					newGLTFMat->SetAlphaCutoff(material->alpha_cutoff);
					newGLTFMat->SetDoubleSidedMode(material->double_sided);

					scene.AddUniqueMaterial(newMat);

					return textureFutures;
				}));
		}

		// Wait until all of the materials and textures are loaded:
		for (size_t matFutureIdx = 0; matFutureIdx < matFutures.size(); matFutureIdx++)
		{
			util::ThreadSafeVector<std::future<void>> const& textureFutures = matFutures[matFutureIdx].get();

			for (size_t textureFutureIdx = 0; textureFutureIdx < textureFutures.size(); textureFutureIdx++)
			{
				textureFutures[textureFutureIdx].wait();
			}
		}
	}


	void SetTransformValues(cgltf_node const* current, entt::entity sceneNode)
	{
		SEAssert((current->has_matrix != (current->has_rotation || current->has_scale || current->has_translation)) ||
			(current->has_matrix == 0 && current->has_rotation == 0 &&
				current->has_scale == 0 && current->has_translation == 0),
			"Transform has both matrix and decomposed properties");

		fr::Transform& targetTransform = fr::SceneNode::GetTransform(*fr::EntityManager::Get(), sceneNode);

		if (current->has_matrix)
		{
			const glm::mat4 nodeModelMatrix = glm::make_mat4(current->matrix);
			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(nodeModelMatrix, scale, rotation, translation, skew, perspective);

			targetTransform.SetLocalRotation(rotation);
			targetTransform.SetLocalScale(scale);
			targetTransform.SetLocalPosition(translation);
		}
		else
		{
			if (current->has_rotation)
			{
				// Note: GLM expects quaternions to be specified in WXYZ order
				targetTransform.SetLocalRotation(
					glm::quat(current->rotation[3], current->rotation[0], current->rotation[1], current->rotation[2]));
			}
			if (current->has_scale)
			{
				targetTransform.SetLocalScale(glm::vec3(current->scale[0], current->scale[1], current->scale[2]));
			}
			if (current->has_translation)
			{
				targetTransform.SetLocalPosition(
					glm::vec3(current->translation[0], current->translation[1], current->translation[2]));
			}
		}
	};


	// Creates a default camera if current == nullptr
	void LoadAddCamera(re::SceneData& scene, entt::entity sceneNode, cgltf_node const* current)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		constexpr char const* k_defaultCamName = "DefaultCamera";
		if (sceneNode == entt::null)
		{
			sceneNode = fr::SceneNode::Create(em, std::format("{}_SceneNode", k_defaultCamName).c_str(), entt::null);
		}


		if (current == nullptr || current->camera == nullptr)
		{
			LOG("Creating a default camera");

			gr::Camera::Config camConfig;

			camConfig.m_aspectRatio = re::RenderManager::Get()->GetWindowAspectRatio();
			camConfig.m_yFOV = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultFOVKey);
			camConfig.m_near = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultNearKey);
			camConfig.m_far = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultFarKey);

			fr::CameraComponent::CreateCameraConcept(
				em,
				sceneNode,
				k_defaultCamName,
				camConfig);
		}
		else
		{
			cgltf_camera const* const camera = current->camera;

			SEAssert(sceneNode != entt::null && camera != nullptr, "Must supply a scene node and camera pointer");

			SetTransformValues(current, sceneNode);

			const std::string camName = camera->name ? std::string(camera->name) : "Unnamed camera";
			LOG("Loading camera \"%s\"", camName.c_str());

			gr::Camera::Config camConfig;
			camConfig.m_projectionType = camera->type == cgltf_camera_type_orthographic ?
				gr::Camera::Config::ProjectionType::Orthographic : gr::Camera::Config::ProjectionType::Perspective;
			if (camConfig.m_projectionType == gr::Camera::Config::ProjectionType::Orthographic)
			{
				camConfig.m_yFOV = 0;
				camConfig.m_near = camera->data.orthographic.znear;
				camConfig.m_far = camera->data.orthographic.zfar;
				camConfig.m_orthoLeftRightBotTop.x = -camera->data.orthographic.xmag / 2.0f;
				camConfig.m_orthoLeftRightBotTop.y = camera->data.orthographic.xmag / 2.0f;
				camConfig.m_orthoLeftRightBotTop.z = -camera->data.orthographic.ymag / 2.0f;
				camConfig.m_orthoLeftRightBotTop.w = camera->data.orthographic.ymag / 2.0f;
			}
			else
			{
				camConfig.m_yFOV = camera->data.perspective.yfov;
				camConfig.m_near = camera->data.perspective.znear;
				camConfig.m_far = camera->data.perspective.zfar;
				camConfig.m_aspectRatio =
					camera->data.perspective.has_aspect_ratio ? camera->data.perspective.aspect_ratio : 1.0f;
				camConfig.m_orthoLeftRightBotTop.x = 0.f;
				camConfig.m_orthoLeftRightBotTop.y = 0.f;
				camConfig.m_orthoLeftRightBotTop.z = 0.f;
				camConfig.m_orthoLeftRightBotTop.w = 0.f;
			}

			// Create the camera and set the transform values on the parent object:
			fr::CameraComponent::CreateCameraConcept(em, sceneNode, camName.c_str(), camConfig);
		}

		em.EnqueueEntityCommand<fr::SetMainCameraCommand>(sceneNode);
	}


	void LoadAddLight(re::SceneData& scene, cgltf_node const* current, entt::entity sceneNode)
	{
		std::string lightName;
		if (current->light->name)
		{
			lightName = std::string(current->light->name);
		}
		else
		{
			static std::atomic<uint32_t> unnamedLightIndex = 0;
			const uint32_t thisLightIndex = unnamedLightIndex.fetch_add(1);
			lightName = "UnnamedLight_" + std::to_string(thisLightIndex);
		}

		LOG("Found light \"%s\"", lightName.c_str());

		// For now we always attach a shadow and let light graphics systems decide to render it or not
		const bool attachShadow = true;

		const glm::vec4 colorIntensity = glm::vec4(
			current->light->color[0],
			current->light->color[1],
			current->light->color[2],
			current->light->intensity);

		fr::EntityManager& em = *fr::EntityManager::Get();

		// The GLTF 2.0 KHR_lights_punctual extension supports directional, point, and spot light types
		switch (current->light->type)
		{
		case cgltf_light_type::cgltf_light_type_directional:
		{
			fr::LightComponent::AttachDeferredDirectionalLightConcept(
				em, sceneNode, lightName, colorIntensity, attachShadow);
		}
		break;
		case cgltf_light_type::cgltf_light_type_point:
		{
			fr::LightComponent::AttachDeferredPointLightConcept(em, sceneNode, lightName, colorIntensity, attachShadow);
		}
		break;
		case cgltf_light_type::cgltf_light_type_spot:
		{
			fr::LightComponent::AttachDeferredSpotLightConcept(em, sceneNode, lightName, colorIntensity, attachShadow);
		}
		break;
		case cgltf_light_type::cgltf_light_type_invalid:
		case cgltf_light_type::cgltf_light_type_max_enum:
		default:
			SEAssertF("Invalid light type");
		}
	}


	void LoadIBLTexture(std::string const& sceneRootPath, re::SceneData& scene)
	{
		// Ambient lights are not supported by GLTF 2.0; Instead, we handle it manually.
		// First, we check for a <sceneRoot>\IBL\ibl.hdr file for per-scene IBLs/skyboxes.
		// If that fails, we fall back to a default HDRI
		// Later, we'll use the IBL texture to generate the IEM and PMREM textures in a GraphicsSystem
		std::shared_ptr<re::Texture> iblTexture = nullptr;

		auto TryLoadIBL = [&scene](std::string const& IBLPath, std::shared_ptr<re::Texture>& iblTexture) {
			if (scene.TextureExists(IBLPath))
			{
				iblTexture = scene.GetTexture(IBLPath);
			}
			else
			{
				iblTexture = grutil::LoadTextureFromFilePath(
					{ IBLPath },
					re::Texture::ColorSpace::Linear,
					false,
					re::Texture::k_errorTextureColor);
			}
			};

		std::string IBLPath;
		if (core::Config::Get()->TryGetValue<std::string>(core::configkeys::k_sceneIBLPathKey, IBLPath))
		{
			TryLoadIBL(IBLPath, iblTexture);
		}

		if (!iblTexture)
		{
			IBLPath = core::Config::Get()->GetValueAsString(core::configkeys::k_defaultEngineIBLPathKey);
			TryLoadIBL(IBLPath, iblTexture);
		}
		SEAssert(iblTexture != nullptr,
			std::format("Missing IBL texture. Per scene IBLs must be placed at {}; A default fallback must exist at {}",
				core::Config::Get()->GetValueAsString(core::configkeys::k_sceneIBLPathKey),
				core::Config::Get()->GetValueAsString(core::configkeys::k_defaultEngineIBLPathKey)).c_str());
	}


	inline gr::MeshPrimitive::TopologyMode CGLTFPrimitiveTypeToGrTopologyMode(cgltf_primitive_type primitiveType)
	{
		switch (primitiveType)
		{
		case cgltf_primitive_type::cgltf_primitive_type_points: return gr::MeshPrimitive::TopologyMode::PointList;
		case cgltf_primitive_type::cgltf_primitive_type_lines: return gr::MeshPrimitive::TopologyMode::LineList;
		case cgltf_primitive_type::cgltf_primitive_type_line_strip: return gr::MeshPrimitive::TopologyMode::LineStrip;
		case cgltf_primitive_type::cgltf_primitive_type_triangles: return gr::MeshPrimitive::TopologyMode::TriangleList;
		case cgltf_primitive_type::cgltf_primitive_type_triangle_strip: return gr::MeshPrimitive::TopologyMode::TriangleStrip;
		case cgltf_primitive_type::cgltf_primitive_type_triangle_fan:
		case cgltf_primitive_type::cgltf_primitive_type_line_loop:
		case cgltf_primitive_type::cgltf_primitive_type_invalid:
		case cgltf_primitive_type::cgltf_primitive_type_max_enum:
		default: SEAssertF("Invalid/unsupported primitive type/draw mode. SE does not support line loops or triangle fans");
		}
		return gr::MeshPrimitive::TopologyMode::TriangleList; // This should never happen
	}


	void LoadMeshGeometry(
		std::string const& sceneRootPath,
		re::SceneData& scene,
		cgltf_node const* current,
		entt::entity sceneNode,
		AnimationNodeToDataMaps const& animationNodeToData)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		std::string meshName;
		if (current->mesh->name)
		{
			meshName = current->mesh->name;
		}
		else
		{
			static std::atomic<uint32_t> unnamedMeshIdx = 0;
			const uint32_t thisMeshIdx = unnamedMeshIdx.fetch_add(1);
			meshName = std::format("UnnamedMesh_{}", thisMeshIdx);
		}

		fr::Mesh::AttachMeshConcept(sceneNode, meshName.c_str());

		// Morph target weights: We'll store these on the associated morph target vertex streams
		std::vector<float> morphWeights; 
		if (current->weights != nullptr) // GLTF specs: Node weights take priority
		{
			SEAssert(current->weights_count > 0, "Node weight count is 0. This is unexpected");

			morphWeights.resize(current->weights_count, 0.f); // GLTF spec: Default target weights are 0
			memcpy(morphWeights.data(), current->weights, sizeof(float) * current->weights_count);
		}
		else if (current->mesh->weights != nullptr) // GLTF specs: Mesh weights used when node weights are undefined
		{
			SEAssert(current->mesh->weights_count > 0, "Mesh weight count is 0. This is unexpected");

			morphWeights.resize(current->mesh->weights_count, 0.f); // GLTF spec: Default target weights are 0
			memcpy(morphWeights.data(), current->mesh->weights, sizeof(float) * current->mesh->weights_count);
		}

		bool meshHasMorphTargets = false;

		// Add each MeshPrimitive as a child of the SceneNode's Mesh:
		const uint32_t numMeshPrimitives = util::CheckedCast<uint32_t>(current->mesh->primitives_count);
		for (size_t primitive = 0; primitive < numMeshPrimitives; primitive++)
		{
			cgltf_primitive const& curPrimitive = current->mesh->primitives[primitive];

			// Populate the mesh params:
			const gr::MeshPrimitive::MeshPrimitiveParams meshPrimitiveParams{
				.m_topologyMode = CGLTFPrimitiveTypeToGrTopologyMode(curPrimitive.type),
			};

			// Index stream:
			SEAssert(curPrimitive.indices != nullptr, "Mesh is missing indices");
			
			const size_t indicesComponentNumBytes = cgltf_component_size(curPrimitive.indices->component_type);
			SEAssert(indicesComponentNumBytes == 2 || indicesComponentNumBytes == 4,
				"Unexpected index component byte size");

			const size_t numIndices = cgltf_accessor_unpack_indices(
				curPrimitive.indices, nullptr, indicesComponentNumBytes, curPrimitive.indices->count);

			util::ByteVector indices = indicesComponentNumBytes == 2 ? 
				util::ByteVector::Create<uint16_t>(numIndices) : 
				util::ByteVector::Create<uint32_t>(numIndices);

			re::VertexStream::DataType indexDataType = re::VertexStream::DataType::DataType_Count;
			switch (indicesComponentNumBytes)
			{
			case 2: // uint16_t
			{
				indexDataType = re::VertexStream::DataType::UShort;
				cgltf_accessor_unpack_indices(
					curPrimitive.indices, indices.data<uint16_t>(), indicesComponentNumBytes, numIndices);
			}
			break;
			case 4: // uint32_t
			{
				indexDataType = re::VertexStream::DataType::UInt;
				cgltf_accessor_unpack_indices(
					curPrimitive.indices, indices.data<uint32_t>(), indicesComponentNumBytes, numIndices);
			}
			break;
			default: SEAssertF("Unexpected number of bytes in indices component");
			}

			re::VertexStream* indexStream = re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Index,
					.m_dataType = indexDataType,
				},
				std::move(indices)).get();
			
			// Vertex streams:
			re::VertexStream* position0Stream = nullptr;
			re::VertexStream* normal0Stream = nullptr;
			re::VertexStream* tangent0Stream = nullptr;
			re::VertexStream* uv0Stream = nullptr;
			re::VertexStream* color0Stream = nullptr;
			// TODO: Caclulate unspecified normals/tangents/uvs for all semantic indexes, morph targets, etc
					
			std::vector<gr::MeshPrimitive::MeshVertexStream> meshPrimitiveVertexStreams;

			std::vector<util::ByteVector*> extraChannelsData;
			std::set<util::ByteVector const*> seenExtraChannelsData; // Only add 1 copy of (potentially reused) data
			auto AddExtraChannelsData = [&seenExtraChannelsData, &extraChannelsData](util::ByteVector* extraChannelData)
				{
					if (!seenExtraChannelsData.contains(extraChannelData))
					{
						extraChannelsData.emplace_back(extraChannelData);
						seenExtraChannelsData.emplace(extraChannelData);
					}
				};

			glm::vec3 positionsMinXYZ(fr::BoundsComponent::k_invalidMinXYZ);
			glm::vec3 positionsMaxXYZ(fr::BoundsComponent::k_invalidMaxXYZ);
		
			// Unpack each of the primitive's vertex attrbutes:
			for (size_t attrib = 0; attrib < curPrimitive.attributes_count; attrib++)
			{
				cgltf_attribute const& curAttribute = curPrimitive.attributes[attrib];

				const size_t numComponents = cgltf_num_components(curAttribute.data->type);

				// GLTF mesh vertex attributes are stored as vecN's only:
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
				SEAssert(numComponents <= 4, "Invalid vertex attribute data type");

				const size_t numElements = curAttribute.data->count;
				const size_t totalFloatElements = numComponents * numElements;

				const uint8_t streamIdx = util::CheckedCast<uint8_t>(curAttribute.index);

				const cgltf_attribute_type vertexAttributeType = curAttribute.type;
				switch (vertexAttributeType)
				{
				case cgltf_attribute_type::cgltf_attribute_type_position:
				{
					util::ByteVector positions = 
						util::ByteVector::Create<glm::vec3>(curAttribute.data->count);
					
					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(positions.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					re::VertexStream* positionStream = re::VertexStream::Create(
						re::VertexStream::CreateParams{
							.m_type = re::VertexStream::Type::Position,
							.m_dataType = re::VertexStream::DataType::Float3,
						},
						std::move(positions)).get();

					if (position0Stream == nullptr) // GLTF only supports 1 position input...
					{
						position0Stream = positionStream;

						// Store our min/max
						if (curAttribute.data->has_min)
						{
							SEAssert(sizeof(curAttribute.data->min) == 64,
								"Unexpected number of bytes in min value array data");

							memcpy(&positionsMinXYZ.x, curAttribute.data->min, sizeof(glm::vec3));
						}
						if (curAttribute.data->has_max)
						{
							SEAssert(sizeof(curAttribute.data->max) == 64,
								"Unexpected number of bytes in max value array data");

							memcpy(&positionsMaxXYZ.x, curAttribute.data->max, sizeof(glm::vec3));
						}
					}
					meshPrimitiveVertexStreams.emplace_back(positionStream, streamIdx);
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_normal:
				{
					util::ByteVector normals = util::ByteVector::Create<glm::vec3>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(normals.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					re::VertexStream* normalStream = re::VertexStream::Create(
						re::VertexStream::CreateParams{
							.m_type = re::VertexStream::Type::Normal,
							.m_dataType = re::VertexStream::DataType::Float3,
							.m_doNormalize = re::VertexStream::Normalize::True,
						},
						std::move(normals)).get();

					if (normal0Stream == nullptr) // GLTF only supports 1 normal input...
					{
						normal0Stream = normalStream;
					}
					meshPrimitiveVertexStreams.emplace_back(normalStream, streamIdx);
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_tangent:
				{
					util::ByteVector tangents = util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(tangents.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					re::VertexStream* tangentStream = re::VertexStream::Create(
						re::VertexStream::CreateParams{
							.m_type = re::VertexStream::Type::Tangent,
							.m_dataType = re::VertexStream::DataType::Float4,
							.m_doNormalize = re::VertexStream::Normalize::True,
						},
						std::move(tangents)).get();

					if (tangent0Stream == nullptr) // GLTF only supports 1 tangent input...
					{
						tangent0Stream = tangentStream;
					}
					meshPrimitiveVertexStreams.emplace_back(tangentStream, streamIdx);
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_texcoord:
				{
					util::ByteVector uvs = util::ByteVector::Create<glm::vec2>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(uvs.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					re::VertexStream* uvStream = re::VertexStream::Create(
						re::VertexStream::CreateParams{
							.m_type = re::VertexStream::Type::TexCoord,
							.m_dataType = re::VertexStream::DataType::Float2,
						},
						std::move(uvs)).get();

					if (uv0Stream == nullptr)
					{
						uv0Stream = uvStream;
					}					
					meshPrimitiveVertexStreams.emplace_back(uvStream, streamIdx);
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_color:
				{
					SEAssert(numComponents == 4, "Only 4-channel colors (RGBA) are (currently) supported");

					util::ByteVector colors =
						util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(colors.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					re::VertexStream* colorStream = re::VertexStream::Create(
						re::VertexStream::CreateParams{
							.m_type = re::VertexStream::Type::Color,
							.m_dataType = re::VertexStream::DataType::Float4,
						},
						std::move(colors)).get();

					if (color0Stream == nullptr)
					{
						color0Stream = colorStream;
					}
					meshPrimitiveVertexStreams.emplace_back(colorStream, streamIdx);
					AddExtraChannelsData(&colorStream->GetDataByteVector());
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_joints: // joints_n == indexes from skin.joints array
				{
					SEAssertF("TODO: Fix this - these are vec4's");

					std::vector<float> jointsAsFloats; // We unpack joints as floats...
					jointsAsFloats.resize(totalFloatElements);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(jointsAsFloats.data()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					util::ByteVector jointsAsUints = 
						util::ByteVector::Create<uint8_t>(totalFloatElements); // ...and convert to uint8_t

					for (size_t jointIdx = 0; jointIdx < jointsAsFloats.size(); jointIdx++)
					{
						// Cast our joint indexes from floats to uint8_t's:
						jointsAsUints.at<uint8_t>(jointIdx) = util::CheckedCast<uint8_t>(jointsAsFloats[jointIdx]);
					}
				
					re::VertexStream* jointStream = re::VertexStream::Create(
						re::VertexStream::CreateParams{
							.m_type = re::VertexStream::Type::BlendIndices,
							.m_dataType = re::VertexStream::DataType::UByte,
						},
						std::move(jointsAsUints)).get();

					meshPrimitiveVertexStreams.emplace_back(jointStream, streamIdx);
					AddExtraChannelsData(&jointStream->GetDataByteVector());
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_weights: // How stronly a joint influences a vertex
				{
					SEAssertF("TODO: Fix this - these are vec4's");

					util::ByteVector weights = 
						util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(weights.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					re::VertexStream* weightStream = re::VertexStream::Create(
						re::VertexStream::CreateParams{
							 .m_type = re::VertexStream::Type::BlendWeight,
							 .m_dataType = re::VertexStream::DataType::Float,
						},
						std::move(weights)).get();

					meshPrimitiveVertexStreams.emplace_back(weightStream, streamIdx);
					AddExtraChannelsData(&weightStream->GetDataByteVector());
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_custom:
				{
					SEAssertF("Custom vertex attributes are not (currently) supported");
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_max_enum:
				case cgltf_attribute_type::cgltf_attribute_type_invalid:
				default:
					SEAssertF("Invalid attribute type");
				}
			} // End vertex attribute unpacking


			// Morph targets:
			// TODO: Use the VertexStreamBuilder to generate any missing morph target data

			auto AddMorphTarget = [&meshPrimitiveVertexStreams](
				re::VertexStream::Type streamType, uint8_t streamTypeIdx, re::VertexStream const* morphTarget)
				{
					bool foundStreamType = false;
					for (auto& meshPrimVertStream : meshPrimitiveVertexStreams)
					{
						if (meshPrimVertStream.m_vertexStream->GetType() == streamType &&
							meshPrimVertStream.m_typeIdx == streamTypeIdx)
						{
							meshPrimVertStream.m_morphTargets.emplace_back(morphTarget);
							foundStreamType = true;
							break;
						}
					}
					SEAssert(foundStreamType, "Failed to find stream type to add morph target to")
				};

			for (size_t targetIdx = 0; targetIdx < curPrimitive.targets_count; ++targetIdx)
			{
				cgltf_morph_target const& curTarget = curPrimitive.targets[targetIdx];
				for (size_t targetAttribIdx = 0; targetAttribIdx < curTarget.attributes_count; ++targetAttribIdx)
				{
					cgltf_attribute const& curTargetAttribute = curTarget.attributes[targetAttribIdx];

					const size_t numTargetFloats = cgltf_accessor_unpack_floats(curTargetAttribute.data, nullptr, 0);

					cgltf_attribute_type targetAttributeType = curTargetAttribute.type;

					util::ByteVector morphData = util::ByteVector::Create<glm::vec3>(curTargetAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curTargetAttribute.data,
						static_cast<float*>(morphData.data<float>()),
						numTargetFloats);
					SEAssert(unpackResult, "Failed to unpack data");

					const uint8_t targetStreamIdx = util::CheckedCast<uint8_t>(curTargetAttribute.index);

					switch (targetAttributeType)
					{
					case cgltf_attribute_type::cgltf_attribute_type_position:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3, "Unexpected data type");

						re::VertexStream* positionMorphTarget = re::VertexStream::Create(
							re::VertexStream::CreateParams{
								 .m_type = re::VertexStream::Type::Position,
								 .m_isMorphData = re::VertexStream::IsMorphData::True,
								 .m_dataType = re::VertexStream::DataType::Float3,
							},
							std::move(morphData)).get();
						
						AddMorphTarget(re::VertexStream::Type::Position, targetStreamIdx, positionMorphTarget);
						AddExtraChannelsData(&positionMorphTarget->GetDataByteVector());
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_normal:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3, "Unexpected data type");

						re::VertexStream* normalMorphTarget = re::VertexStream::Create(
							re::VertexStream::CreateParams{
								 .m_type = re::VertexStream::Type::Normal,
								 .m_isMorphData = re::VertexStream::IsMorphData::True,
								 .m_dataType = re::VertexStream::DataType::Float3,
							},
							std::move(morphData)).get();

						AddMorphTarget(re::VertexStream::Type::Normal, targetStreamIdx, normalMorphTarget);
						AddExtraChannelsData(&normalMorphTarget->GetDataByteVector());
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_tangent:
					{
						// Note: Tangent morph targets are vec3's
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3, "Unexpected data type");

						re::VertexStream* tangentMorphTarget = re::VertexStream::Create(
							re::VertexStream::CreateParams{
								 .m_type = re::VertexStream::Type::Tangent,
								 .m_isMorphData = re::VertexStream::IsMorphData::True,
								 .m_dataType = re::VertexStream::DataType::Float3,
							},
							std::move(morphData)).get();

						AddMorphTarget(re::VertexStream::Type::Tangent, targetStreamIdx, tangentMorphTarget);
						AddExtraChannelsData(&tangentMorphTarget->GetDataByteVector());
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_texcoord:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec2, "Unexpected data type");

						re::VertexStream* uvMorphTarget = re::VertexStream::Create(
							re::VertexStream::CreateParams{
								 .m_type = re::VertexStream::Type::TexCoord,
								 .m_isMorphData = re::VertexStream::IsMorphData::True,
								 .m_dataType = re::VertexStream::DataType::Float2,
							},
							std::move(morphData)).get();

						AddMorphTarget(re::VertexStream::Type::TexCoord, targetStreamIdx, uvMorphTarget);
						AddExtraChannelsData(&uvMorphTarget->GetDataByteVector());
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_color:
					{
						// Note: Color morph target data can be a vec3 or vec4
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec4,
							"Unexpected data type. Only 4-color RGBA is currently supported");

						re::VertexStream* colorMorphTarget = re::VertexStream::Create(
							re::VertexStream::CreateParams{
								 .m_type = re::VertexStream::Type::Color,
								 .m_isMorphData = re::VertexStream::IsMorphData::True,
								 .m_dataType = re::VertexStream::DataType::Float4,
							},
							std::move(morphData)).get();

						AddMorphTarget(re::VertexStream::Type::Color, targetStreamIdx, colorMorphTarget);
						AddExtraChannelsData(&colorMorphTarget->GetDataByteVector());
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_joints:
					case cgltf_attribute_type::cgltf_attribute_type_weights:
					{
						SEAssertF("Invalid attribute type for morph target data");
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_custom:
					{
						SEAssertF("Custom vertex attributes are not (currently) supported");
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_max_enum:
					case cgltf_attribute_type::cgltf_attribute_type_invalid:
					default: SEAssertF("Invalid attribute type");
					}
				}
			}


			// Create empty objects for anything the VertexStreamBuilder can create:
			std::vector<util::ByteVector> missingStreamData;
			missingStreamData.reserve(re::VertexStream::k_maxVertexStreams); // Referenced by ptr: Ensure we don't re-allocate

			util::ByteVector* normal0Data = nullptr;
			if (normal0Stream == nullptr)
			{
				normal0Data = &missingStreamData.emplace_back(util::ByteVector::Create<glm::vec3>());
			}
			else
			{
				normal0Data = &normal0Stream->GetDataByteVector();
			}

			util::ByteVector* tangent0Data = nullptr;
			if (tangent0Stream == nullptr)
			{
				tangent0Data = &missingStreamData.emplace_back(util::ByteVector::Create<glm::vec4>());
			}
			else
			{
				tangent0Data = &tangent0Stream->GetDataByteVector();
			}

			util::ByteVector* uv0Data = nullptr;
			if (uv0Stream == nullptr)
			{
				uv0Data = &missingStreamData.emplace_back(util::ByteVector::Create<glm::vec2>());
			}
			else
			{
				uv0Data = &uv0Stream->GetDataByteVector();
			}

			// Construct any missing vertex attributes for the mesh:
			grutil::VertexStreamBuilder::MeshData meshData
			{
				meshName,
				&meshPrimitiveParams,
				&indexStream->GetDataByteVector(),
				&position0Stream->GetDataByteVector(),
				normal0Data,
				tangent0Data,
				uv0Data,
				extraChannelsData.data(),
				extraChannelsData.size(),
			};
			grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);		


			// Finally, create any missing vertex streams using generated data:
			if (normal0Stream == nullptr)
			{
				normal0Stream = re::VertexStream::Create(
					re::VertexStream::CreateParams{
						.m_type = re::VertexStream::Type::Normal,
						.m_dataType = re::VertexStream::DataType::Float3,
						.m_doNormalize = re::VertexStream::Normalize::True,
					},
					std::move(*normal0Data)).get();

				meshPrimitiveVertexStreams.emplace_back(normal0Stream);
			}
			if (tangent0Stream == nullptr)
			{
				tangent0Stream = re::VertexStream::Create(
					re::VertexStream::CreateParams{
						.m_type = re::VertexStream::Type::Tangent,
						.m_dataType = re::VertexStream::DataType::Float4,
						.m_doNormalize = re::VertexStream::Normalize::True,
					},
					std::move(*tangent0Data)).get();

				meshPrimitiveVertexStreams.emplace_back(tangent0Stream);
			}
			if (uv0Stream == nullptr)
			{
				uv0Stream = re::VertexStream::Create(
					re::VertexStream::CreateParams{
						.m_type = re::VertexStream::Type::TexCoord,
						.m_dataType = re::VertexStream::DataType::Float2,
					},
					std::move(*uv0Data)).get();

				meshPrimitiveVertexStreams.emplace_back(uv0Stream);
			}
			if (color0Stream == nullptr)
			{
				// Note: We create the color byte vector and fill it with defaults now, no need to pass it through the
				// vertex stream builder
				color0Stream = re::VertexStream::Create(
					re::VertexStream::CreateParams{
						.m_type = re::VertexStream::Type::Color,
						.m_dataType = re::VertexStream::DataType::Float4
					},
					util::ByteVector::Create<glm::vec4>(
						position0Stream->GetNumElements(), glm::vec4(1.f) /*= GLTF default*/)).get();

				meshPrimitiveVertexStreams.emplace_back(color0Stream);
			}


			// Construct the MeshPrimitive (internally registers itself with the SceneData):
			std::shared_ptr<gr::MeshPrimitive> newMeshPrimitive = gr::MeshPrimitive::Create(
				meshName.c_str(),
				indexStream,
				std::move(meshPrimitiveVertexStreams),
				meshPrimitiveParams);

			// Attach the MeshPrimitive to the Mesh:
			entt::entity meshPrimimitiveEntity = fr::MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
				em,
				sceneNode,
				newMeshPrimitive.get(),
				positionsMinXYZ,
				positionsMaxXYZ);

			// Assign a material:
			std::shared_ptr<gr::Material> material;
			if (curPrimitive.material != nullptr)
			{
				const std::string generatedMatName = grutil::GenerateMaterialName(*curPrimitive.material);
				material = scene.GetMaterial(generatedMatName);
			}
			else
			{
				LOG_WARNING("MeshPrimitive \"%s\" does not have a material. Assigning \"%s\"",
					meshName.c_str(), en::DefaultResourceNames::k_missingMaterialName);
				material = scene.GetMaterial(en::DefaultResourceNames::k_missingMaterialName);
			}
			fr::MaterialInstanceComponent::AttachMaterialComponent(em, meshPrimimitiveEntity, material.get());
		} // primitives loop


		// Attach a MeshAnimationComponent, if necessary:
		if (meshHasMorphTargets)
		{
#if defined(_DEBUG)
			bool hasWeightsAnimation = false;
			for (auto const& animation : animationNodeToData)
			{
				if (animation.contains(current))
				{
					for (auto const& channel : animation.at(current).m_channels)
					{
						if (channel.m_targetPath == fr::AnimationPath::Weights)
						{
							hasWeightsAnimation = true;
							break;
						}
					}
				}
			}
			SEAssert(hasWeightsAnimation, 
				"Current node contains morph targets, but does not have any animation that targets its morph weights");
#endif

			fr::MeshAnimationComponent::AttachMeshAnimationComponent(em, sceneNode);
		}
	}


	inline fr::InterpolationMode CGLTFInterpolationTypeToFrInterpolationType(
		cgltf_interpolation_type interpolationType,
		cgltf_animation_path_type animationPathType)
	{
		switch (interpolationType)
		{
		case cgltf_interpolation_type::cgltf_interpolation_type_linear:
		{
			if (animationPathType == cgltf_animation_path_type_rotation)
			{
				return fr::InterpolationMode::SphericalLinearInterpolation;
			}
			return fr::InterpolationMode::Linear;
		}
		case cgltf_interpolation_type::cgltf_interpolation_type_step: return fr::InterpolationMode::Step;
		case cgltf_interpolation_type::cgltf_interpolation_type_cubic_spline: return fr::InterpolationMode::CubicSpline;
		default: SEAssertF("Invalid interpolation type");
		}
		return fr::InterpolationMode::Linear; // This should never happen
	}


	inline fr::AnimationPath CGLTFAnimationPathToFrAnimationPath(cgltf_animation_path_type pathType)
	{
		switch (pathType)
		{
		case cgltf_animation_path_type::cgltf_animation_path_type_translation: return fr::AnimationPath::Translation;
		case cgltf_animation_path_type::cgltf_animation_path_type_rotation: return fr::AnimationPath::Rotation;
		case cgltf_animation_path_type::cgltf_animation_path_type_scale: return fr::AnimationPath::Scale;
		case cgltf_animation_path_type::cgltf_animation_path_type_weights: return fr::AnimationPath::Weights;
		case cgltf_animation_path_type::cgltf_animation_path_type_invalid:
		case cgltf_animation_path_type::cgltf_animation_path_type_max_enum:
		default: SEAssertF("Invalid animation path type");
		}
		return fr::AnimationPath::Translation; // This should never happen
	}


	void AttachAnimationComponents(
		cgltf_node const* current,
		entt::entity curSceneNodeEntity,
		fr::AnimationController const* animationController,
		AnimationNodeToDataMaps const& nodeToAnimationDataMap)
	{
		SEAssert(animationController, "animationController cannot be null");

		fr::EntityManager& em = *fr::EntityManager::Get();
		SEAssert(!em.HasComponent<fr::AnimationComponent>(curSceneNodeEntity), "Node already has an animation component");

		fr::AnimationComponent* animationCmpt = 
			fr::AnimationComponent::AttachAnimationComponent(em, curSceneNodeEntity, animationController);

		// Attach each/all animations that target the current node to its animation component:
		for (auto const& animation : nodeToAnimationDataMap)
		{
			if (!animation.contains(current))
			{
				continue;
			}
			
			animationCmpt->SetAnimationData(animation.at(current));
		}
	}


	void LoadObjectHierarchyRecursiveHelper(
		std::string const& sceneRootPath,
		re::SceneData& scene,
		cgltf_data const* data,
		cgltf_node const* current,
		entt::entity curSceneNodeEntity,
		fr::AnimationController const* animationController,
		AnimationNodeToDataMaps const& animationNodeToData,
		std::vector<std::future<void>>& loadTasks)
	{
		SEAssert(current != nullptr, "We should not be traversing into null nodes");

		SEAssert(current->light == nullptr || current->mesh == nullptr,
			"TODO: Handle nodes with multiple things (eg. Light & Mesh) that depend on a transform");
		// TODO: Seems we never hit this... Does GLTF support multiple attachments per node?

		if (current->children_count > 0) // Depth-first traversal
		{
			for (size_t i = 0; i < current->children_count; i++)
			{
				std::string const& nodeName = current->name ? current->name : "Unnamed child node";

				entt::entity childNode =
					fr::SceneNode::Create(*fr::EntityManager::Get(), nodeName.c_str(), curSceneNodeEntity);

				LoadObjectHierarchyRecursiveHelper(
					sceneRootPath,
					scene,
					data,
					current->children[i],
					childNode,
					animationController,
					animationNodeToData,
					loadTasks);
			}
		}

		// Set the SceneNode transform:
		loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob([current, curSceneNodeEntity]()
			{
				SetTransformValues(current, curSceneNodeEntity);
			}));

		// Process node attachments:
		if (current->mesh)
		{
			loadTasks.emplace_back(
				core::ThreadPool::Get()->EnqueueJob([&sceneRootPath, &scene, current, curSceneNodeEntity, &animationNodeToData]()
					{
						LoadMeshGeometry(sceneRootPath, scene, current, curSceneNodeEntity, animationNodeToData);
					}));
		}
		if (current->light)
		{
			loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob([&scene, current, curSceneNodeEntity]()
				{
					LoadAddLight(scene, current, curSceneNodeEntity);
				}));
		}
		if (current->camera)
		{
			loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob([&scene, current, curSceneNodeEntity]()
				{
					LoadAddCamera(scene, curSceneNodeEntity, current);
				}));
		}
		// Node animations: Check each of our animations, if we find a single animation that targets a node we know we
		// must attach an animation component to it
		for (auto const& entry : animationNodeToData)
		{
			if (entry.contains(current))
			{
				loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(
					[data, animationController, &animationNodeToData, current, curSceneNodeEntity]()
					{
						AttachAnimationComponents(current, curSceneNodeEntity, animationController, animationNodeToData);
					}));
				break;
			}
		}
	}


	AnimationNodeToDataMaps LoadAnimationData(
		std::string const& sceneFilePath, cgltf_data const* data, fr::AnimationController*& animationControllerOut)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		animationControllerOut = fr::AnimationController::CreateAnimationController(em, sceneFilePath.c_str());

		// Each element/index corresponds to an animation: Multiple animations may target the same node
		AnimationNodeToDataMaps animationNodeToData;

		for (uint64_t animIdx = 0; animIdx < data->animations_count; ++animIdx)
		{
			std::string animationName;
			if (data->animations[animIdx].name)
			{
				animationName = data->animations[animIdx].name;
			}
			else
			{
				static std::atomic<uint32_t> unnamedAnimationIdx = 0;
				animationName = std::format("UnnamedAnimation_{}", unnamedAnimationIdx.fetch_add(1));
			}
			LOG("Loading animation \"%s\"...", animationName.c_str());

			animationControllerOut->AddNewAnimation(animationName.c_str());

			// Pack the Channels of an AnimationData struct:
			std::unordered_map<cgltf_node const*, fr::AnimationData>& nodeToData = animationNodeToData.emplace_back();
			for (uint64_t channelIdx = 0; channelIdx < data->animations->channels_count; ++channelIdx)
			{
				// GLTF animation samplers define an "input/output pair":
				// - A set of floating-point scalar values representing linear time in seconds
				// - A set of vectors or scalars representing the animated property
				// 
				// Note: The GLTF specifications also mandate that within 1 animation, each target (i.e. target node and
				// animation path) MUST NOT be used more than once
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations

				cgltf_animation_sampler const* animSampler = data->animations[animIdx].channels[channelIdx].sampler;

				// Get/create a new AnimationData structure:
				cgltf_node const* targetNode = data->animations[animIdx].channels[channelIdx].target_node;
				fr::AnimationData* animationData = nullptr;
				if (nodeToData.contains(targetNode))
				{
					animationData = &nodeToData.at(targetNode);
				}
				else
				{
					animationData = &nodeToData.emplace(targetNode, fr::AnimationData{}).first->second;
				}

				// Create a new animation channel entry:
				fr::AnimationData::Channel& animChannel = animationData->m_channels.emplace_back();

				// Channel interpolation mode:
				animChannel.m_interpolationMode = CGLTFInterpolationTypeToFrInterpolationType(
						animSampler->interpolation,
						data->animations[animIdx].channels[channelIdx].target_path);

				// Channel target path:
				animChannel.m_targetPath = 
					CGLTFAnimationPathToFrAnimationPath(data->animations[animIdx].channels[channelIdx].target_path);

				// Channel input data: (Linear keyframe times, in seconds)
				const cgltf_size numKeyframeTimeEntries = cgltf_accessor_unpack_floats(animSampler->input, nullptr, 0);

				std::vector<float> keyframeTimesSec(numKeyframeTimeEntries);
				cgltf_accessor_unpack_floats(animSampler->input, keyframeTimesSec.data(), numKeyframeTimeEntries);

				animChannel.m_keyframeTimesIdx = animationControllerOut->AddKeyframeTimes(std::move(keyframeTimesSec));

				// Channel output data:
				const cgltf_size numOutputFloats = cgltf_accessor_unpack_floats(animSampler->output, nullptr, 0);

				std::vector<float> outputFloatData(numOutputFloats);
				cgltf_accessor_unpack_floats(animSampler->output, outputFloatData.data(), numOutputFloats);

				animChannel.m_dataIdx = animationControllerOut->AddChannelData(std::move(outputFloatData));

				SEAssert(numOutputFloats % numKeyframeTimeEntries == 0,
					"The number of keyframe entries must be an exact multiple of the number of output floats");

				animChannel.m_dataFloatsPerKeyframe = util::CheckedCast<uint8_t>(numOutputFloats / numKeyframeTimeEntries);
			}
		}
		return animationNodeToData;
	}


	// Note: data must already be populated by calling cgltf_load_buffers
	void LoadSceneHierarchy(
		std::string const& sceneFilePath, std::string const& sceneRootPath, re::SceneData& scene, cgltf_data* data)
	{
		LOG("Scene has %d object nodes", data->nodes_count);

		SEAssert(data->scenes_count == 1, "Loading > 1 scene is currently unsupported");

		std::vector<std::future<void>> loadTasks; // Task enqueuing is single-threaded

		// Create an animation controller for the scene, and pre-process the animation data:
		fr::AnimationController* animationController = nullptr;
		AnimationNodeToDataMaps const& nodeToAnimationData = LoadAnimationData(sceneFilePath, data, animationController);

		// Each node is the root in a transformation hierarchy:
		for (size_t node = 0; node < data->scenes->nodes_count; node++)
		{
			SEAssert(data->scenes->nodes[node]->parent == nullptr, "Error: Node is not a root");

			const std::string nodeName =
				data->scenes->nodes[node]->name ? data->scenes->nodes[node]->name : "Unnamed root node";

			LOG("Loading root node %zu: \"%s\"", node, nodeName.c_str());

			entt::entity rootSceneNode =
				fr::SceneNode::Create(*fr::EntityManager::Get(), nodeName.c_str(), entt::null); // Root has no parent

			LoadObjectHierarchyRecursiveHelper(
				sceneRootPath,
				scene,
				data,
				data->scenes->nodes[node],
				rootSceneNode,
				animationController,
				nodeToAnimationData,
				loadTasks);
		}

		// Wait for all of the tasks to be done:
		for (size_t loadTask = 0; loadTask < loadTasks.size(); loadTask++)
		{
			loadTasks[loadTask].wait();
		}
	}
} // namespace


namespace fr
{
	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<fr::SceneManager> instance = std::make_unique<fr::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager()
		: m_sceneRenderSystemNameID(core::INamedObject::ComputeIDFromName(k_sceneRenderSystemName))
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		util::PerformanceTimer timer;
		timer.Start();

		std::string sceneFilePath;
		core::Config::Get()->TryGetValue<std::string>(core::configkeys::k_sceneFilePathKey, sceneFilePath);

		const bool loadResult = Load(sceneFilePath);
		if (!loadResult)
		{
			LOG_ERROR("Failed to load scene from path \"%s\"", sceneFilePath.c_str());
		}
		else
		{
			LOG("\nSceneManager successfully loaded scene \"%s\" in %f seconds\n", 
				sceneFilePath.c_str(), timer.StopSec());
		}


		// Create a scene render system:
		class CreateSceneRenderSystemCommand
		{
		public:
			CreateSceneRenderSystemCommand() = default;
			~CreateSceneRenderSystemCommand() = default;

			static void Execute(void* cmdData)
			{
				CreateSceneRenderSystemCommand* cmdPtr = reinterpret_cast<CreateSceneRenderSystemCommand*>(cmdData);

				std::string pipelineFileName;
				if (core::Config::Get()->TryGetValue(core::configkeys::k_scenePipelineCmdLineArg, pipelineFileName) == false)
				{
					pipelineFileName = core::configkeys::k_defaultScenePipelineFileName;
				}

				re::RenderSystem const* sceneRenderSystem =
					re::RenderManager::Get()->CreateAddRenderSystem(k_sceneRenderSystemName, pipelineFileName);
			}

			static void Destroy(void* cmdData)
			{
				CreateSceneRenderSystemCommand* cmdPtr = reinterpret_cast<CreateSceneRenderSystemCommand*>(cmdData);
				cmdPtr->~CreateSceneRenderSystemCommand();
			}

		private:

		};
		re::RenderManager::Get()->EnqueueRenderCommand<CreateSceneRenderSystemCommand>();
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		//
	}


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// 
	}


	void SceneManager::ShowImGuiWindow(bool* show) const
	{
		if (!*show)
		{
			return;
		}

		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Scene Manager";
		ImGui::Begin(k_panelTitle, show);

		if (ImGui::CollapsingHeader("Spawn Entities", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();

			enum EntityType : uint8_t
			{
				Light,
				Mesh,

				EntityType_Count
			};
			constexpr std::array<char const*, EntityType::EntityType_Count> k_entityTypeNames = {
				"Light",
				"Mesh"
			};
			static_assert(k_entityTypeNames.size() == EntityType::EntityType_Count);

			constexpr ImGuiComboFlags k_comboFlags = 0;

			static EntityType s_selectedEntityTypeIdx = static_cast<EntityType>(0);
			const EntityType currentSelectedEntityTypeIdx = s_selectedEntityTypeIdx;
			if (ImGui::BeginCombo("Entity type", k_entityTypeNames[s_selectedEntityTypeIdx], k_comboFlags))
			{
				for (uint8_t comboIdx = 0; comboIdx < k_entityTypeNames.size(); comboIdx++)
				{
					const bool isSelected = comboIdx == s_selectedEntityTypeIdx;
					if (ImGui::Selectable(k_entityTypeNames[comboIdx], isSelected))
					{
						s_selectedEntityTypeIdx = static_cast<EntityType>(comboIdx);
					}

					// Set the initial focus:
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}


			ImGui::Separator();


			switch (s_selectedEntityTypeIdx)
			{
			case EntityType::Light:
			{
				fr::LightComponent::ShowImGuiSpawnWindow();
			}
			break;
			case EntityType::Mesh:
			{
				fr::Mesh::ShowImGuiSpawnWindow();
			}
			break;
			default: SEAssertF("Invalid EntityType");
			}

			ImGui::Unindent();
		}

		ImGui::End();
	}


	bool SceneManager::Load(std::string const& sceneFilePath)
	{
		//stbi_set_flip_vertically_on_load(false); // Set this once. Note: It is NOT thread safe, and must be consistent

		// Start by parsing the the GLTF file metadata:
		const bool gotSceneFilePath = !sceneFilePath.empty();
		cgltf_options options = { (cgltf_file_type)0 };
		cgltf_data* data = nullptr;
		if (gotSceneFilePath)
		{
			cgltf_result parseResult = cgltf_parse_file(&options, sceneFilePath.c_str(), &data);
			if (parseResult != cgltf_result::cgltf_result_success)
			{
				SEAssert(parseResult == cgltf_result_success, "Failed to parse scene file \"" + sceneFilePath + "\"");
				return false;
			}
		}

		std::vector<std::future<void>> earlyLoadTasks;

		re::SceneData* sceneData = re::RenderManager::GetSceneData();

		earlyLoadTasks.emplace_back(
			core::ThreadPool::Get()->EnqueueJob([&sceneData]() {
				GenerateDefaultResources(*sceneData);
				}));

		// Add a default camera to start:
		earlyLoadTasks.emplace_back(
			core::ThreadPool::Get()->EnqueueJob([&sceneData]() {
				LoadAddCamera(*sceneData, entt::null, nullptr);
				}));

		// Load the IBL/skybox HDRI:
		std::string sceneRootPath;
		core::Config::Get()->TryGetValue<std::string>("sceneRootPath", sceneRootPath);
		earlyLoadTasks.emplace_back(
			core::ThreadPool::Get()->EnqueueJob([&sceneData, &sceneRootPath]() {
				LoadIBLTexture(sceneRootPath, *sceneData);
				}));

		// Start loading the GLTF file data:
		if (data)
		{
			cgltf_result bufferLoadResult = cgltf_load_buffers(&options, data, sceneFilePath.c_str());
			if (bufferLoadResult != cgltf_result::cgltf_result_success)
			{
				SEAssert(bufferLoadResult == cgltf_result_success, "Failed to load scene data \"" + sceneFilePath + "\"");
				return false;
			}

#if defined(_DEBUG)
			cgltf_result validationResult = cgltf_validate(data);
			if (validationResult != cgltf_result::cgltf_result_success)
			{
				SEAssert(validationResult == cgltf_result_success, "GLTF file failed validation!");
				return false;
			}
#endif

			// Load the materials first:
			PreLoadMaterials(sceneRootPath, *sceneData, data);

			// Load the scene hierarchy:
			LoadSceneHierarchy(sceneFilePath, sceneRootPath, *sceneData, data);

			// Cleanup:
			cgltf_free(data);
		}

		// Wait for all of the tasks we spawned here to be done:
		for (size_t loadTask = 0; loadTask < earlyLoadTasks.size(); loadTask++)
		{
			earlyLoadTasks[loadTask].wait();
		}

		sceneData->EndLoading();

		return true;
	}
}