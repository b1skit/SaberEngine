// © 2022 Adam Badke. All rights reserved.
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
					static_cast<re::Texture::Usage>(re::Texture::Usage::Color | re::Texture::Usage::ComputeTarget),
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
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::Color | re::Texture::Usage::ComputeTarget),
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
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::Color | re::Texture::Usage::ComputeTarget),
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


	void SetTransformValues(cgltf_node* current, entt::entity sceneNode)
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
	void LoadAddCamera(re::SceneData& scene, entt::entity sceneNode, cgltf_node* current)
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


	void LoadAddLight(re::SceneData& scene, cgltf_node* current, entt::entity sceneNode)
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


	void LoadMeshGeometry(
		std::string const& sceneRootPath, re::SceneData& scene, cgltf_node* current, entt::entity sceneNode)
	{
		std::string meshName;
		if (current->mesh->name)
		{
			meshName = std::string(current->mesh->name);
		}
		else
		{
			static std::atomic<uint32_t> unnamedMeshIdx = 0;
			const uint32_t thisMeshIdx = unnamedMeshIdx.fetch_add(1);
			meshName = "UnnamedMesh_" + std::to_string(thisMeshIdx);
		}

		const uint32_t numMeshPrimitives = util::CheckedCast<uint32_t>(current->mesh->primitives_count);

		fr::Mesh::AttachMeshConcept(sceneNode, meshName.c_str());

		// Add each MeshPrimitive as a child of the SceneNode's Mesh:
		for (size_t primitive = 0; primitive < numMeshPrimitives; primitive++)
		{
			// Populate the mesh params:
			gr::MeshPrimitive::MeshPrimitiveParams meshPrimitiveParams;
			switch (current->mesh->primitives[primitive].type)
			{
			case cgltf_primitive_type::cgltf_primitive_type_points:
			{
				meshPrimitiveParams.m_topologyMode = gr::MeshPrimitive::TopologyMode::PointList;
			}
			break;
			case cgltf_primitive_type::cgltf_primitive_type_lines:
			{
				meshPrimitiveParams.m_topologyMode = gr::MeshPrimitive::TopologyMode::LineList;
			}
			break;
			case cgltf_primitive_type::cgltf_primitive_type_line_strip:
			{
				meshPrimitiveParams.m_topologyMode = gr::MeshPrimitive::TopologyMode::LineStrip;
			}
			break;
			case cgltf_primitive_type::cgltf_primitive_type_triangles:
			{
				meshPrimitiveParams.m_topologyMode = gr::MeshPrimitive::TopologyMode::TriangleList;
			}
			break;
			case cgltf_primitive_type::cgltf_primitive_type_triangle_strip:
			{
				meshPrimitiveParams.m_topologyMode = gr::MeshPrimitive::TopologyMode::TriangleStrip;
			}
			break;
			case cgltf_primitive_type::cgltf_primitive_type_triangle_fan:
			case cgltf_primitive_type::cgltf_primitive_type_line_loop:
			case cgltf_primitive_type::cgltf_primitive_type_max_enum:
			default:
				SEAssertF("Unsupported primitive type/draw mode. Saber Engine does not support line loops or triangle fans");
				meshPrimitiveParams.m_topologyMode = gr::MeshPrimitive::TopologyMode::TriangleList;
			}

			SEAssert(current->mesh->primitives[primitive].indices != nullptr, "Mesh is missing indices");
			std::vector<uint32_t> indices;
			indices.resize(current->mesh->primitives[primitive].indices->count, 0);
			for (size_t index = 0; index < current->mesh->primitives[primitive].indices->count; index++)
			{
				// Note: We use 32-bit indexes, but cgltf uses size_t's
				indices[index] = (uint32_t)cgltf_accessor_read_index(
					current->mesh->primitives[primitive].indices, (uint64_t)index);
			}

			// Unpack each of the primitive's vertex attrbutes:
			std::vector<float> positions;
			glm::vec3 positionsMinXYZ(fr::BoundsComponent::k_invalidMinXYZ);
			glm::vec3 positionsMaxXYZ(fr::BoundsComponent::k_invalidMaxXYZ);
			std::vector<float> normals;
			std::vector<float> tangents;
			std::vector<float> uv0;
			bool foundUV0 = false; // TODO: Support minimum of 2 UV sets. For now, just use the 1st
			std::vector<float> colors;
			std::vector<float> jointsAsFloats; // We unpack the joints as floats...
			std::vector<uint8_t> jointsAsUints; // ...but eventually convert and store them as uint8_t
			std::vector<float> weights;
			for (size_t attrib = 0; attrib < current->mesh->primitives[primitive].attributes_count; attrib++)
			{
				size_t elementsPerComponent;
				switch (current->mesh->primitives[primitive].attributes[attrib].data->type)
				{
				case cgltf_type::cgltf_type_scalar:
				{
					elementsPerComponent = 1;
				}
				break;
				case cgltf_type::cgltf_type_vec2:
				{
					elementsPerComponent = 2;
				}
				break;
				case cgltf_type::cgltf_type_vec3:
				{
					elementsPerComponent = 3;
				}
				break;
				case cgltf_type::cgltf_type_vec4:
				{
					elementsPerComponent = 4;
				}
				break;
				case cgltf_type::cgltf_type_mat2:
				case cgltf_type::cgltf_type_mat3:
				case cgltf_type::cgltf_type_mat4:
				case cgltf_type::cgltf_type_max_enum:
				case cgltf_type::cgltf_type_invalid:
				default:
				{
					// GLTF mesh vertex attributes are stored as vecN's only:
					// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
					SEAssertF("Invalid vertex attribute data type");
				}
				}
				const size_t numComponents = current->mesh->primitives[primitive].attributes[attrib].data->count;
				const size_t totalFloatElements = numComponents * elementsPerComponent;

				float* dataTarget = nullptr;
				const cgltf_attribute_type attributeType =
					current->mesh->primitives[primitive].attributes[attrib].type;
				switch (attributeType)
				{
				case cgltf_attribute_type::cgltf_attribute_type_position:
				{
					positions.resize(totalFloatElements, 0);
					dataTarget = &positions[0];

					if (current->mesh->primitives[primitive].attributes[attrib].data->has_min)
					{
						SEAssert(sizeof(current->mesh->primitives[primitive].attributes[attrib].data->min) == 64,
							"Unexpected number of bytes in min value array data");

						float* xyzComponent = current->mesh->primitives[primitive].attributes[attrib].data->min;
						positionsMinXYZ.x = *xyzComponent++;
						positionsMinXYZ.y = *xyzComponent++;
						positionsMinXYZ.z = *xyzComponent;
					}
					if (current->mesh->primitives[primitive].attributes[attrib].data->has_max)
					{
						SEAssert(sizeof(current->mesh->primitives[primitive].attributes[attrib].data->max) == 64,
							"Unexpected number of bytes in max value array data");

						float* xyzComponent = current->mesh->primitives[primitive].attributes[attrib].data->max;
						positionsMaxXYZ.x = *xyzComponent++;
						positionsMaxXYZ.y = *xyzComponent++;
						positionsMaxXYZ.z = *xyzComponent;
					}
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_normal:
				{
					normals.resize(totalFloatElements, 0);
					dataTarget = &normals[0];
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_tangent:
				{
					tangents.resize(totalFloatElements, 0);
					dataTarget = &tangents[0];
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_texcoord:
				{
					// TODO: Support minimum of 2 UV sets. For now, just use the 1st set encountered
					if (foundUV0)
					{
						LOG_WARNING("MeshPrimitive \"%s\" contains >1 UV set. Currently, only a single UV channel is "
							"supported, subsequent sets are ignored", meshName.c_str());
						continue;
					}
					foundUV0 = true;
					uv0.resize(totalFloatElements, 0);
					dataTarget = &uv0[0];
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_color:
				{
					SEAssert(elementsPerComponent == 4, "Only 4-channel colors (RGBA) are currently supported");
					colors.resize(totalFloatElements, 0);
					dataTarget = &colors[0];
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_joints:
				{
					LOG_WARNING("Found vertex joint attributes: Data will be loaded but has not been tested. "
						"Skinning is not currently supported");
					jointsAsFloats.resize(totalFloatElements, 0);
					jointsAsUints.resize(totalFloatElements, 0);
					dataTarget = &jointsAsFloats[0];
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_weights:
				{
					LOG_WARNING("Found vertex weight attributes: Data will be loaded but has not been tested. "
						"Skinning is not currently supported");
					weights.resize(totalFloatElements, 0);
					dataTarget = &weights[0];
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_custom:
				case cgltf_attribute_type::cgltf_attribute_type_max_enum:
				case cgltf_attribute_type::cgltf_attribute_type_invalid:
				default:
					SEAssertF("Invalid attribute type");
				}

				cgltf_accessor* const accessor = current->mesh->primitives[primitive].attributes[attrib].data;

				bool unpackResult = cgltf_accessor_unpack_floats(
					accessor,
					dataTarget,
					totalFloatElements);
				SEAssert(unpackResult, "Failed to unpack data");

				// Post-process the data:
				if (attributeType == cgltf_attribute_type_joints)
				{
					// Cast our joint indexes from floats to uint8_t's:
					SEAssert(jointsAsFloats.size() == jointsAsUints.size(), "Source/destination size mismatch");
					for (size_t jointIdx = 0; jointIdx < jointsAsFloats.size(); jointIdx++)
					{
						jointsAsUints[jointIdx] = static_cast<uint8_t>(jointsAsFloats[jointIdx]);
					}
				}
			} // End attribute unpacking

			// Construct any missing vertex attributes for the mesh:
			grutil::VertexStreamBuilder::MeshData meshData
			{
				meshName,
				&meshPrimitiveParams,
				&indices,
				reinterpret_cast<std::vector<glm::vec3>*>(&positions),
				reinterpret_cast<std::vector<glm::vec3>*>(&normals),
				reinterpret_cast<std::vector<glm::vec4>*>(&tangents),
				reinterpret_cast<std::vector<glm::vec2>*>(&uv0),
				reinterpret_cast<std::vector<glm::vec4>*>(&colors),
				reinterpret_cast<std::vector<glm::tvec4<uint8_t>>*>(&jointsAsUints),
				reinterpret_cast<std::vector<glm::vec4>*>(&weights)
			};
			grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

			// Construct the MeshPrimitive (internally registers itself with the SceneData):
			std::shared_ptr<gr::MeshPrimitive> meshPrimitiveSceneData = gr::MeshPrimitive::Create(
				meshName.c_str(),
				&indices,
				positions,
				&normals,
				&tangents,
				&uv0,
				&colors,
				&jointsAsUints,
				&weights,
				meshPrimitiveParams);

			fr::EntityManager& em = *fr::EntityManager::Get();

			// Attach the MeshPrimitive to the Mesh:
			entt::entity meshPrimimitiveEntity = fr::MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
				em,
				sceneNode,
				meshPrimitiveSceneData.get(),
				positionsMinXYZ,
				positionsMaxXYZ);

			// Assign a material:
			std::shared_ptr<gr::Material> material;
			if (current->mesh->primitives[primitive].material != nullptr)
			{
				const std::string generatedMatName =
					grutil::GenerateMaterialName(*current->mesh->primitives[primitive].material);
				material = scene.GetMaterial(generatedMatName);
			}
			else
			{
				LOG_WARNING("MeshPrimitive \"%s\" does not have a material. Assigning \"%s\"",
					meshName.c_str(), en::DefaultResourceNames::k_missingMaterialName);
				material = scene.GetMaterial(en::DefaultResourceNames::k_missingMaterialName);
			}
			fr::MaterialInstanceComponent::AttachMaterialComponent(em, meshPrimimitiveEntity, material.get());
		}
	}


	void LoadObjectHierarchyRecursiveHelper(
		std::string const& sceneRootPath,
		re::SceneData& scene,
		cgltf_data* data,
		cgltf_node* current,
		entt::entity parentSceneNode,
		std::vector<std::future<void>>& loadTasks)
	{
		if (current == nullptr)
		{
			SEAssertF("We should not be traversing into null nodes");
			return;
		}

		SEAssert(current->light == nullptr || current->mesh == nullptr,
			"TODO: Handle nodes with multiple things (eg. Light & Mesh) that depend on a transform");
		// TODO: Seems we never hit this... Does GLTF support multiple attachments per node?

		if (current->children_count > 0) // Depth-first traversal
		{
			for (size_t i = 0; i < current->children_count; i++)
			{
				std::string const& nodeName = current->name ? current->name : "Unnamed child node";

				entt::entity childNode =
					fr::SceneNode::Create(*fr::EntityManager::Get(), nodeName.c_str(), parentSceneNode);

				LoadObjectHierarchyRecursiveHelper(
					sceneRootPath, scene, data, current->children[i], childNode, loadTasks);
			}
		}

		// Set the SceneNode transform:
		loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob([current, parentSceneNode]()
			{
				SetTransformValues(current, parentSceneNode);
			}));

		// Process node attachments:
		if (current->mesh)
		{
			loadTasks.emplace_back(
				core::ThreadPool::Get()->EnqueueJob([&sceneRootPath, &scene, current, parentSceneNode]()
					{
						LoadMeshGeometry(sceneRootPath, scene, current, parentSceneNode);
					}));
		}
		if (current->light)
		{
			loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob([&scene, current, parentSceneNode]()
				{
					LoadAddLight(scene, current, parentSceneNode);
				}));
		}
		if (current->camera)
		{
			loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob([&scene, current, parentSceneNode]()
				{
					LoadAddCamera(scene, parentSceneNode, current);
				}));
		}
	}


	// Note: data must already be populated by calling cgltf_load_buffers
	void LoadSceneHierarchy(std::string const& sceneRootPath, re::SceneData& scene, cgltf_data* data)
	{
		LOG("Scene has %d object nodes", data->nodes_count);

		SEAssert(data->scenes_count == 1, "Loading > 1 scene is currently unsupported");

		std::vector<std::future<void>> loadTasks; // Task enqueuing is single-threaded

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
				sceneRootPath, scene, data, data->scenes->nodes[node], rootSceneNode, loadTasks);
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
	const NameID SceneManager::k_sceneRenderSystemNameID = core::INamedObject::ComputeIDFromName(k_sceneRenderSystemName);


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

			// TODO: Add a cmd line flag to validated GLTF files, for efficiency?
			cgltf_result validationResult = cgltf_validate(data);
			if (validationResult != cgltf_result::cgltf_result_success)
			{
				SEAssert(validationResult == cgltf_result_success, "GLTF file failed validation!");
				return false;
			}

			// Load the materials first:
			PreLoadMaterials(sceneRootPath, *sceneData, data);

			// Load the scene hierarchy:
			LoadSceneHierarchy(sceneRootPath, *sceneData, data);

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