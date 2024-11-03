// © 2022 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "BoundsComponent.h"
#include "Camera.h"
#include "CameraComponent.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshMorphComponent.h"
#include "MeshPrimitiveComponent.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "SkinningComponent.h"
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
	// Each element/index corresponds to an animation: Multiple animations may target the same node
	using NodeToAnimationDataMaps = std::vector<std::unordered_map<cgltf_node const*, fr::AnimationData>>;

	// We pre-parse the GLTF scene hierarchy into our EnTT registry, and then update the entities later on
	using NodeToEntityMap = std::unordered_map<cgltf_node const*, entt::entity>;


	struct SceneMetadata
	{
		fr::AnimationController* m_animationController = nullptr;
		NodeToAnimationDataMaps m_nodeToAnimationData;

		NodeToEntityMap m_nodeToEntity;
	};


	util::ByteVector UnpackColorAttributeAsVec4(cgltf_attribute const& colorAttribute)
	{
		SEAssert(colorAttribute.type == cgltf_attribute_type::cgltf_attribute_type_color,
			"Attribute is not a color attribute");

		const size_t numComponents = cgltf_num_components(colorAttribute.data->type);
		const size_t numElements = colorAttribute.data->count;
		const size_t totalFloatElements = numComponents * numElements;

		util::ByteVector colors = util::ByteVector::Create<glm::vec4>(colorAttribute.data->count);

		switch (numComponents)
		{
		case 3:
		{
			std::vector<glm::vec3> tempColors(colorAttribute.data->count);

			const bool unpackResult = cgltf_accessor_unpack_floats(
				colorAttribute.data,
				&tempColors[0].r,
				totalFloatElements);
			SEAssert(unpackResult, "Failed to unpack data");

			for (size_t colIdx = 0; colIdx < tempColors.size(); ++colIdx)
			{
				// GLTF specs: Color attributes of vec3 type are assumed to have an alpha of 1
				colors.at<glm::vec4>(colIdx) = glm::vec4(tempColors[colIdx], 1.f);
			}
		}
		break;
		case 4:
		{
			const bool unpackResult = cgltf_accessor_unpack_floats(
				colorAttribute.data,
				static_cast<float*>(colors.data<float>()),
				totalFloatElements);
			SEAssert(unpackResult, "Failed to unpack data");
		}
		break;
		default: SEAssertF("Invalid number of color components");
		}

		return colors;
	}


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
			std::string const& fallbackName =
				grutil::GenerateTextureColorFallbackName(colorFallback, numChannels, colorSpace);

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
		LOG("Generating a default GLTF pbrMetallicRoughness material \"%s\"...",
			en::DefaultResourceNames::k_defaultGLTFMaterialName);

		std::shared_ptr<gr::Material> defaultMaterialGLTF = gr::Material::Create(
			en::DefaultResourceNames::k_defaultGLTFMaterialName,
			gr::Material::EffectMaterial::GLTF_PBRMetallicRoughness);

		constexpr uint8_t k_defaultUVChannelIdx = 0;

		// BaseColorTex
		defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::BaseColor, 
			grutil::LoadTextureFromFilePath(
				{ en::DefaultResourceNames::k_defaultAlbedoTexName },
				re::Texture::ColorSpace::sRGB,
				true,
				glm::vec4(1.f)), // White
			k_defaultUVChannelIdx);

		// MetallicRoughnessTex
		defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::MetallicRoughness, 
			grutil::LoadTextureFromFilePath(
				{ en::DefaultResourceNames::k_defaultMetallicRoughnessTexName },
				re::Texture::ColorSpace::Linear,
				true,
				glm::vec4(0.f, 1.f, 1.f, 0.f)), // GLTF specs: .BG = metalness, roughness, Default: .BG = 1, 1
			k_defaultUVChannelIdx);

		// NormalTex
		defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::Normal, 
			grutil::LoadTextureFromFilePath(
				{ en::DefaultResourceNames::k_defaultNormalTexName },
				re::Texture::ColorSpace::Linear,
				true,
				glm::vec4(0.5f, 0.5f, 1.f, 0.f)),
			k_defaultUVChannelIdx);

		// OcclusionTex
		defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::Occlusion,
			grutil::LoadTextureFromFilePath(
				{ en::DefaultResourceNames::k_defaultOcclusionTexName },
				re::Texture::ColorSpace::Linear,
				true,
				glm::vec4(1.f)),
			k_defaultUVChannelIdx);

		// EmissiveTex
		defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::Emissive,
			grutil::LoadTextureFromFilePath(
				{ en::DefaultResourceNames::k_defaultEmissiveTexName },
				re::Texture::ColorSpace::sRGB,
				true,
				glm::vec4(0.f)),
			k_defaultUVChannelIdx);

		scene.AddUniqueMaterial(defaultMaterialGLTF);


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
		// Note: We must load our images through materials in order to infer the format and color space of the data

		const size_t numMaterials = data->materials_count;
		LOG("Loading %d scene materials", numMaterials);

		// We assign each material to a thread; These threads will spawn new threads to load each texture. We need to 
		// wait on the future of each material to know when we can begin waiting on the futures for its textures
		std::vector<std::future<util::ThreadSafeVector<std::future<void>>>> matFutures;
		matFutures.reserve(numMaterials);

		for (size_t matIdx = 0; matIdx < numMaterials; matIdx++)
		{
			matFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob([data, matIdx, &scene, &sceneRootPath]()
				-> util::ThreadSafeVector<std::future<void>>
				{

					util::ThreadSafeVector<std::future<void>> textureFutures;
					textureFutures.reserve(5); // BaseColor, met/rough, normal, occlusion, emissive

					cgltf_material const* const material = &data->materials[matIdx];
					SEAssert(material, "Found a null material, this is unexpected");

					std::string const& matName = grutil::GenerateMaterialName(*material);
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
						gr::Material::Create(matName, gr::Material::EffectMaterial::GLTF_PBRMetallicRoughness);

					// GLTF specifications: If a texture is not given, all texture components are assumed to be 1.f
					// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
					constexpr glm::vec4 k_defaultTextureColor(1.f, 1.f, 1.f, 1.f);

					// BaseColorTex
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &k_defaultTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(gr::Material_GLTF::TextureSlotIdx::BaseColor,
								LoadTextureOrColor(
									scene,
									sceneRootPath,
									material->pbr_metallic_roughness.base_color_texture.texture,
									k_defaultTextureColor,
									re::Texture::Format::RGBA8_UNORM,
									re::Texture::ColorSpace::sRGB),
								material->pbr_metallic_roughness.base_color_texture.texcoord);
						}));

					// MetallicRoughnessTex
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &k_defaultTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(gr::Material_GLTF::TextureSlotIdx::MetallicRoughness,
								LoadTextureOrColor(
									scene,
									sceneRootPath,
									material->pbr_metallic_roughness.metallic_roughness_texture.texture,
									k_defaultTextureColor,
									re::Texture::Format::RGBA8_UNORM,
									re::Texture::ColorSpace::Linear),
								material->pbr_metallic_roughness.metallic_roughness_texture.texcoord);
						}));

					// NormalTex
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &k_defaultTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(gr::Material_GLTF::TextureSlotIdx::Normal,
								LoadTextureOrColor(
									scene,
									sceneRootPath,
									material->normal_texture.texture,
									glm::vec4(0.5f, 0.5f, 1.0f, 0.0f), // Equivalent to a [0,0,1] normal after unpacking
									re::Texture::Format::RGBA8_UNORM,
									re::Texture::ColorSpace::Linear),
								material->normal_texture.texcoord);
						}));

					// OcclusionTex
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &k_defaultTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(gr::Material_GLTF::TextureSlotIdx::Occlusion, 
								LoadTextureOrColor(
									scene,
									sceneRootPath,
									material->occlusion_texture.texture,
									k_defaultTextureColor,	// Completely unoccluded
									re::Texture::Format::RGBA8_UNORM,
									re::Texture::ColorSpace::Linear),
								material->occlusion_texture.texcoord);
						}));

					// EmissiveTex
					textureFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[newMat, &k_defaultTextureColor, &scene, &sceneRootPath, material]() {
							newMat->SetTexture(gr::Material_GLTF::TextureSlotIdx::Emissive,
								LoadTextureOrColor(
									scene,
									sceneRootPath,
									material->emissive_texture.texture,
									k_defaultTextureColor,
									re::Texture::Format::RGBA8_UNORM,
									re::Texture::ColorSpace::sRGB), // GLTF spec: Must be converted to linear before use
								material->emissive_texture.texcoord); 
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

		fr::EntityManager& em = *fr::EntityManager::Get();
		SEAssert(em.HasComponent<fr::TransformComponent>(sceneNode), "Entity does not have a TransformComponent");

		fr::Transform& targetTransform = em.GetComponent<fr::TransformComponent>(sceneNode).GetTransform();

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
			if (current->has_scale)
			{
				targetTransform.SetLocalScale(glm::vec3(current->scale[0], current->scale[1], current->scale[2]));
			}
			if (current->has_rotation)
			{
				// Note: GLM expects quaternions to be specified in WXYZ order
				targetTransform.SetLocalRotation(
					glm::quat(current->rotation[3], current->rotation[0], current->rotation[1], current->rotation[2]));
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
			fr::TransformComponent::AttachTransformComponent(em, sceneNode);
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

			char const* camName = camera->name ? camera->name : "Unnamed camera";
			LOG("Loading camera \"%s\"", camName);

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
			fr::CameraComponent::CreateCameraConcept(em, sceneNode, camName, camConfig);
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


	inline gr::MeshPrimitive::PrimitiveTopology CGLTFPrimitiveTypeToPrimitiveTopology(cgltf_primitive_type primitiveType)
	{
		switch (primitiveType)
		{
		case cgltf_primitive_type::cgltf_primitive_type_points: return gr::MeshPrimitive::PrimitiveTopology::PointList;
		case cgltf_primitive_type::cgltf_primitive_type_lines: return gr::MeshPrimitive::PrimitiveTopology::LineList;
		case cgltf_primitive_type::cgltf_primitive_type_line_strip: return gr::MeshPrimitive::PrimitiveTopology::LineStrip;
		case cgltf_primitive_type::cgltf_primitive_type_triangles: return gr::MeshPrimitive::PrimitiveTopology::TriangleList;
		case cgltf_primitive_type::cgltf_primitive_type_triangle_strip: return gr::MeshPrimitive::PrimitiveTopology::TriangleStrip;
		case cgltf_primitive_type::cgltf_primitive_type_triangle_fan:
		case cgltf_primitive_type::cgltf_primitive_type_line_loop:
		case cgltf_primitive_type::cgltf_primitive_type_invalid:
		case cgltf_primitive_type::cgltf_primitive_type_max_enum:
		default: SEAssertF("Invalid/unsupported primitive type/draw mode. SE does not support line loops or triangle fans");
		}
		return gr::MeshPrimitive::PrimitiveTopology::TriangleList; // This should never happen
	}


	void LoadMeshGeometry(
		re::SceneData& scene,
		cgltf_node const* current,
		entt::entity sceneNode,
		SceneMetadata const& sceneMetadata)
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

		// Note: We must defer vertex stream creation until after we've processed the data with the VertexStreamBuilder,
		// as we use a data hash to identify duplicate streams for sharing/reuse

		bool meshHasMorphTargets = false;

		// Add each MeshPrimitive as a child of the SceneNode's Mesh:
		const uint32_t numMeshPrimitives = util::CheckedCast<uint32_t>(current->mesh->primitives_count);
		for (size_t primitive = 0; primitive < numMeshPrimitives; primitive++)
		{
			cgltf_primitive const& curPrimitive = current->mesh->primitives[primitive];

			bool meshPrimitiveIsSkinned = false;

			// Populate the mesh params:
			const gr::MeshPrimitive::MeshPrimitiveParams meshPrimitiveParams{
				.m_primitiveTopology = CGLTFPrimitiveTypeToPrimitiveTopology(curPrimitive.type),
			};

			// Vertex streams:
			// Each vector element corresponds to the m_setIdx of the entries in the array elements
			std::vector<std::array<gr::VertexStream::CreateParams,
				gr::VertexStream::Type::Type_Count>> vertexStreamCreateParams;

			auto AddVertexStreamCreateParams = [&vertexStreamCreateParams](
				gr::VertexStream::CreateParams&& streamCreateParams)
				{
					// Insert enough elements to make our set index valid:
					while (vertexStreamCreateParams.size() <= streamCreateParams.m_setIdx)
					{
						vertexStreamCreateParams.emplace_back();
					}

					const size_t streamTypeIdx = static_cast<size_t>(streamCreateParams.m_streamDesc.m_type);

					SEAssert(vertexStreamCreateParams.at(
						streamCreateParams.m_setIdx)[streamTypeIdx].m_streamData == nullptr,
						"Stream data is not null, this suggests we've already populated this slot");

					vertexStreamCreateParams.at(streamCreateParams.m_setIdx)[streamTypeIdx] =
						std::move(streamCreateParams);
				};


			// Index stream:
			if (curPrimitive.indices != nullptr)
			{
				const size_t indicesComponentNumBytes = cgltf_component_size(curPrimitive.indices->component_type);
				SEAssert(indicesComponentNumBytes == 1 ||
					indicesComponentNumBytes == 2 || 
					indicesComponentNumBytes == 4,
					"Unexpected index component byte size");

				const size_t numIndices = cgltf_accessor_unpack_indices(
					curPrimitive.indices, nullptr, indicesComponentNumBytes, curPrimitive.indices->count);

				util::ByteVector indices = (indicesComponentNumBytes == 1 || indicesComponentNumBytes == 2) ?
					util::ByteVector::Create<uint16_t>(numIndices) : // We'll expand 8 -> 16 bits
					util::ByteVector::Create<uint32_t>(numIndices);

				re::DataType indexDataType = re::DataType::DataType_Count;
				switch (indicesComponentNumBytes)
				{
				case 1: // uint8_t -> uint16_t
				{
					// DX12 does not support 8 bit indices; Here we expand 8 -> 16 bits
					indexDataType = re::DataType::UShort;

					std::vector<uint8_t> tempIndices(numIndices);
					cgltf_accessor_unpack_indices(
						curPrimitive.indices, tempIndices.data(), indicesComponentNumBytes, numIndices);

					for (size_t i = 0; i < tempIndices.size(); ++i)
					{
						indices.at<uint16_t>(i) = static_cast<uint16_t>(tempIndices[i]);
					}
				}
				break;
				case 2: // uint16_t
				{
					indexDataType = re::DataType::UShort;
					cgltf_accessor_unpack_indices(
						curPrimitive.indices, indices.data<uint16_t>(), indicesComponentNumBytes, numIndices);
				}
				break;
				case 4: // uint32_t
				{
					indexDataType = re::DataType::UInt;
					cgltf_accessor_unpack_indices(
						curPrimitive.indices, indices.data<uint32_t>(), indicesComponentNumBytes, numIndices);
				}
				break;
				default: SEAssertF("Unexpected number of bytes in indices component");
				}

				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(std::move(indices)),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::Index,
						.m_dataType = indexDataType,
					},
					.m_setIdx = 0 // Index stream is always in set 0
					});
			}
		
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

				const uint8_t setIdx = util::CheckedCast<uint8_t>(curAttribute.index);

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

					SEAssert(vertexStreamCreateParams.empty() ||
						vertexStreamCreateParams[0][gr::VertexStream::Type::Position].m_streamData == nullptr,
						"Only a single position stream is supported");
					
					SEAssert(setIdx == 0, "Unexpected stream index for position stream");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(positions)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Position,
							.m_dataType = re::DataType::Float3,
						},
						.m_setIdx = setIdx
					});

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
				break;
				case cgltf_attribute_type::cgltf_attribute_type_normal:
				{
					util::ByteVector normals = util::ByteVector::Create<glm::vec3>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(normals.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(normals)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Normal,
							.m_dataType = re::DataType::Float3,
							.m_doNormalize = gr::VertexStream::Normalize::True,
						},
						.m_setIdx = setIdx
					});
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

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(tangents)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Tangent,
							.m_dataType = re::DataType::Float4,
							.m_doNormalize = gr::VertexStream::Normalize::True,
						},
						.m_setIdx = setIdx
					});
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

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(uvs)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::TexCoord,
							.m_dataType = re::DataType::Float2,
						},
						.m_setIdx = setIdx
					});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_color:
				{
					util::ByteVector colors = UnpackColorAttributeAsVec4(curAttribute);

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
							.m_streamData = std::make_unique<util::ByteVector>(std::move(colors)),
							.m_streamDesc = gr::VertexStream::StreamDesc{
								.m_type = gr::VertexStream::Type::Color,
								.m_dataType = re::DataType::Float4,
							},
							.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_joints: // joints_n == indexes from skin.joints array
				{
					// GLTF specs: Max 4 joints (per set) can influence 1 vertex; Joints are stored as vec4's of 
					// unsigned bytes/shorts
					util::ByteVector joints = util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(joints.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");
				
					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(joints)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::BlendIndices,
							.m_dataType = re::DataType::Float4,
						},
						.m_setIdx = setIdx
					});

					meshPrimitiveIsSkinned = true;
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_weights: // How stronly a joint influences a vertex
				{
					// Weights are stored as vec4's of unsigned bytes/shorts
					util::ByteVector weights = util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(weights.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(weights)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							 .m_type = gr::VertexStream::Type::BlendWeight,
							 .m_dataType = re::DataType::Float4,
						},
						.m_setIdx = setIdx
					});

					meshPrimitiveIsSkinned = true;
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
			auto AddMorphCreateParams = [&vertexStreamCreateParams](
				uint8_t setIdx, gr::VertexStream::Type streamType, gr::VertexStream::MorphData&& morphCreateParams)
				{
					SEAssert(setIdx < vertexStreamCreateParams.size(),
						"Trying to add a morph target to a vertex stream that does not exist");

					std::vector<gr::VertexStream::MorphData>& morphTargetData = 
						vertexStreamCreateParams[setIdx][streamType].m_morphTargetData;

					morphTargetData.emplace_back(std::move(morphCreateParams));
				};

			meshHasMorphTargets |= curPrimitive.targets_count > 0;

			for (size_t targetIdx = 0; targetIdx < curPrimitive.targets_count; ++targetIdx)
			{
				cgltf_morph_target const& curTarget = curPrimitive.targets[targetIdx];
				for (size_t targetAttribIdx = 0; targetAttribIdx < curTarget.attributes_count; ++targetAttribIdx)
				{
					cgltf_attribute const& curTargetAttribute = curTarget.attributes[targetAttribIdx];

					const size_t numTargetFloats = cgltf_accessor_unpack_floats(curTargetAttribute.data, nullptr, 0);

					const uint8_t targetStreamIdx = util::CheckedCast<uint8_t>(curTargetAttribute.index);

					cgltf_attribute_type targetAttributeType = curTargetAttribute.type;
					switch (targetAttributeType)
					{
					case cgltf_attribute_type::cgltf_attribute_type_position:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3, "Unexpected data type");

						util::ByteVector posMorphData = 
							util::ByteVector::Create<glm::vec3>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(posMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx, 
							gr::VertexStream::Position,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(posMorphData)),
								.m_dataType = re::DataType::Float3});
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_normal:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3, "Unexpected data type");

						util::ByteVector normalMorphData =
							util::ByteVector::Create<glm::vec3>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(normalMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::Normal,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(normalMorphData)),
								.m_dataType = re::DataType::Float3});
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_tangent:
					{
						// Note: Tangent morph targets are vec3's
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3, "Unexpected data type");

						util::ByteVector tangentMorphData =
							util::ByteVector::Create<glm::vec3>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(tangentMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::Tangent,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(tangentMorphData)),
								.m_dataType = re::DataType::Float3});
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_texcoord:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec2, "Unexpected data type");

						util::ByteVector uvMorphData =
							util::ByteVector::Create<glm::vec2>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(uvMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::TexCoord,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(uvMorphData)),
								.m_dataType = re::DataType::Float2});
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_color:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3 || 
							curTargetAttribute.data->type == cgltf_type::cgltf_type_vec4,
							"Unexpected data type");

						util::ByteVector morphColors = UnpackColorAttributeAsVec4(curTargetAttribute);

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::Color,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(morphColors)),
								.m_dataType = re::DataType::Float4});
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


			// Create empty containers for anything the VertexStreamBuilder can create.
			// Note: GLTF only supports a single position/normal/tangent (but multiple UV channels etc)
			const bool hasIndices = 
				vertexStreamCreateParams[0][gr::VertexStream::Index].m_streamData != nullptr;
			const bool hasNormal0 =
				vertexStreamCreateParams[0][gr::VertexStream::Normal].m_streamData != nullptr;
			const bool hasTangent0 =
				vertexStreamCreateParams[0][gr::VertexStream::Tangent].m_streamData != nullptr;
			const bool hasUV0 =
				vertexStreamCreateParams[0][gr::VertexStream::TexCoord].m_streamData != nullptr;
			const bool hasColor = vertexStreamCreateParams[0][gr::VertexStream::Color].m_streamData != nullptr;

			if (!hasIndices)
			{
				const size_t numPositions = 
					vertexStreamCreateParams[0][gr::VertexStream::Position].m_streamData->size();

				std::unique_ptr<util::ByteVector> indexData;
				re::DataType indexDataType = re::DataType::DataType_Count;
				if (numPositions < std::numeric_limits<uint16_t>::max())
				{
					indexData = std::make_unique<util::ByteVector>(util::ByteVector::Create<uint16_t>());
					indexDataType = re::DataType::UShort;
				}
				else
				{
					indexData = std::make_unique<util::ByteVector>(util::ByteVector::Create<uint32_t>());
					indexDataType = re::DataType::UInt;
				}			

				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::move(indexData),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::Index,
						.m_dataType = indexDataType,
					},
					.m_setIdx = 0,
				});
			}
			if (!hasNormal0)
			{
				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(util::ByteVector::Create<glm::vec3>()),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Normal,
							.m_dataType = re::DataType::Float3,
							.m_doNormalize = gr::VertexStream::Normalize::True,
						},
						.m_setIdx = 0,
					});
			}
			if (!hasTangent0)
			{
				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(util::ByteVector::Create<glm::vec4>()),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::Tangent,
						.m_dataType = re::DataType::Float4,
						.m_doNormalize = gr::VertexStream::Normalize::True,
					},
					.m_setIdx = 0,
					});
			}
			if (!hasUV0)
			{
				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(util::ByteVector::Create<glm::vec2>()),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::TexCoord,
						.m_dataType = re::DataType::Float2,
					},
					.m_setIdx = 0,
					});
			}
			if (!hasColor) // SE (currently) expects at least 1 color channel
			{
				const size_t numPositionVerts = 
					vertexStreamCreateParams[0][gr::VertexStream::Position].m_streamData->size();

				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(
						util::ByteVector::Create<glm::vec4>(numPositionVerts, glm::vec4(1.f) /*= GLTF default*/)),
					.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Color,
							.m_dataType = re::DataType::Float4,
						},
					.m_setIdx = 0,
					});
			}

			// Assemble the data for the VertexStreamBuilder:					
			std::vector<util::ByteVector*> extraChannelsData;
			extraChannelsData.reserve(vertexStreamCreateParams.size());
			for (auto& streams : vertexStreamCreateParams)
			{
				for (auto& stream : streams)
				{
					if (stream.m_streamData == nullptr)
					{
						continue;
					}

					switch (stream.m_streamDesc.m_type)
					{
					case gr::VertexStream::Index:
					{
						SEAssert(stream.m_setIdx == 0, "Found an index stream beyond index 0. This is unexpected");
						continue;
					}
					break;
					case gr::VertexStream::Color:
					case gr::VertexStream::BlendIndices:
					case gr::VertexStream::BlendWeight:
					{
						extraChannelsData.emplace_back(stream.m_streamData.get());
					}
					break;
					case gr::VertexStream::TexCoord:
					case gr::VertexStream::Position:
					case gr::VertexStream::Normal:
					case gr::VertexStream::Tangent:
					{
						// Position0/Normal0/Tangent0/UV0 are handled elsewhere; But we do add their morph data below
						if (stream.m_setIdx > 0)
						{
							extraChannelsData.emplace_back(stream.m_streamData.get());
						}
					}
					break;
					case gr::VertexStream::Binormal:
					{
						SEAssertF("Binormal streams are nto supported by GLTF, this is unexpected");
					}
					break;
					default: SEAssertF("Invalid stream type");
					}

					// Add any morph target data
					if (!stream.m_morphTargetData.empty())
					{
						for (auto const& morphData : stream.m_morphTargetData)
						{
							extraChannelsData.emplace_back(morphData.m_displacementData.get());							
						}
					}
				}
			}

			// If our mesh has morph targets, add the extra structured usage flag to the animated vertex stream buffers
			if (meshHasMorphTargets)
			{
				for (auto& streamIndexElement : vertexStreamCreateParams)
				{
					for (auto& createParams : streamIndexElement)
					{
						if (createParams.m_streamDesc.m_type != gr::VertexStream::Index &&
							!createParams.m_morphTargetData.empty())
						{
							createParams.m_extraUsageBits |= re::Buffer::Usage::Structured;
						}
					}
				}
			}
			
			// Construct any missing vertex attributes for the mesh:
			grutil::VertexStreamBuilder::MeshData meshData
			{
				.m_name = meshName,
				.m_meshParams = &meshPrimitiveParams,
				.m_indices = vertexStreamCreateParams[0][gr::VertexStream::Index].m_streamData.get(),
				.m_positions = vertexStreamCreateParams[0][gr::VertexStream::Position].m_streamData.get(),
				.m_normals = vertexStreamCreateParams[0][gr::VertexStream::Normal].m_streamData.get(),
				.m_tangents = vertexStreamCreateParams[0][gr::VertexStream::Tangent].m_streamData.get(),
				.m_UV0 = vertexStreamCreateParams[0][gr::VertexStream::TexCoord].m_streamData.get(),
				.m_extraChannels = &extraChannelsData,
			};
			grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

			// TODO: Bug here - Internally, gr::MeshPrimitive creates re::VertexStreams which are backed by an 
			// re::Buffer. Buffers interact with the re::BufferAllocator, which is only allowed from the render thread. 
			// We get away with it for by using a nasty hack to defer the VertexStream's buffer creation onto a render
			// command. This will go away once async loading/object creation is done
			std::shared_ptr<gr::MeshPrimitive> newMeshPrimitive = gr::MeshPrimitive::Create(
				meshName,
				std::move(vertexStreamCreateParams),
				meshPrimitiveParams,
				true);

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
					meshName.c_str(), en::DefaultResourceNames::k_defaultGLTFMaterialName);
				material = scene.GetMaterial(en::DefaultResourceNames::k_defaultGLTFMaterialName);
			}
			fr::MaterialInstanceComponent::AttachMaterialComponent(em, meshPrimimitiveEntity, material.get());

			// Attach a SkinningComponent:
			if (meshPrimitiveIsSkinned)
			{
				// Build our joint index to TransformID mapping table:
				std::vector<gr::TransformID> jointToTransformIDs;
				jointToTransformIDs.reserve(current->skin->joints_count);

				for (size_t jointIdx = 0; jointIdx < current->skin->joints_count; ++jointIdx)
				{
					SEAssert(sceneMetadata.m_nodeToEntity.contains(current->skin->joints[jointIdx]),
						"Node is not in the node to entity map. This should not be possible");

					const entt::entity jointNodeEntity = sceneMetadata.m_nodeToEntity.at(current->skin->joints[jointIdx]);

					jointToTransformIDs.emplace_back(
						em.GetComponent<fr::TransformComponent>(jointNodeEntity).GetTransformID());
				}
				fr::SkinningComponent::AttachSkinningComponent(meshPrimimitiveEntity, std::move(jointToTransformIDs));
			}
		} // primitives loop


		// Attach a MeshMorphComponent, if necessary:
		if (meshHasMorphTargets)
		{
			// GLTF specs: The default target mesh.weights is optional, and must be used when node.weights is null
			float const* defaultWeights = current->weights;
			size_t defaultWeightsCount = current->weights_count;
			if (!defaultWeights)
			{
				defaultWeights = current->mesh->weights;
				defaultWeightsCount = current->mesh->weights_count;
			}

			fr::MeshMorphComponent::AttachMeshAnimationComponent(
				em, sceneNode, current->mesh->weights, util::CheckedCast<uint32_t>(current->mesh->weights_count));
		}
	}


	inline entt::entity CreateSceneNode(cgltf_node const* gltfNode, entt::entity parent)
	{
		constexpr char const* k_unnamedNodeName = "Unnamed node";
		char const* nodeName = gltfNode->name ? gltfNode->name : k_unnamedNodeName;

		fr::EntityManager& em = *fr::EntityManager::Get();

		entt::entity newSceneNode = fr::SceneNode::Create(em, nodeName, parent);

		if (gltfNode->has_translation ||
			gltfNode->has_rotation ||
			gltfNode->has_scale ||
			gltfNode->has_matrix)
		{
			fr::TransformComponent::AttachTransformComponent(em, newSceneNode);
			SetTransformValues(gltfNode, newSceneNode);
		}

		return newSceneNode;
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
		SceneMetadata const& sceneMetadata)
	{
		SEAssert(sceneMetadata.m_animationController, "animationController cannot be null");

		fr::EntityManager& em = *fr::EntityManager::Get();
		SEAssert(!em.HasComponent<fr::AnimationComponent>(curSceneNodeEntity), "Node already has an animation component");

		fr::AnimationComponent* animationCmpt = 
			fr::AnimationComponent::AttachAnimationComponent(em, curSceneNodeEntity, sceneMetadata.m_animationController);

		// Attach each/all animations that target the current node to its animation component:
		for (auto const& animation : sceneMetadata.m_nodeToAnimationData)
		{
			if (!animation.contains(current))
			{
				continue;
			}
			
			animationCmpt->SetAnimationData(animation.at(current));
		}
	}


	void AttachNodeComponents(
		re::SceneData& sceneData,
		cgltf_data const* data,
		SceneMetadata& sceneMetadata,
		std::vector<std::future<void>>& loadTasks)
	{
		for (size_t nodeIdx = 0; nodeIdx < data->nodes_count; ++nodeIdx)
		{
			cgltf_node const* current = &data->nodes[nodeIdx];

			loadTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(
				[&sceneData, &sceneMetadata, current]()
				{
					SEAssert(sceneMetadata.m_nodeToEntity.contains(current),
					"Node to entity map does not contain the current node. This should not be possible");

					const entt::entity curSceneNodeEntity = sceneMetadata.m_nodeToEntity.at(current);

					if (current->mesh)
					{
						LoadMeshGeometry(sceneData, current, curSceneNodeEntity, sceneMetadata);
					}
					if (current->light)
					{
						LoadAddLight(sceneData, current, curSceneNodeEntity);
					}
					if (current->camera)
					{
						LoadAddCamera(sceneData, curSceneNodeEntity, current);
					}

					// Attach an animation component, if it is required:
					bool hasAnimation = current->weights || (current->mesh && current->mesh->weights);
					if (!hasAnimation)
					{
						// Also need to search the node animations
						for (auto const& animation : sceneMetadata.m_nodeToAnimationData)
						{
							if (animation.contains(current))
							{
								hasAnimation = true;
							}
						}
					}

					if (hasAnimation)
					{
						SEAssert((current->weights == nullptr && (!current->mesh || !current->mesh->weights)) ||
							(current->weights && current->weights_count > 0) ||
							(current->mesh && current->mesh->weights && current->mesh->weights_count > 0),
							"Mesh weights count is non-zero, but weights is null");

						AttachAnimationComponents(current, curSceneNodeEntity, sceneMetadata);
					}
				}));
		}
	}


	void CreateSceneNodeEntities(cgltf_data const* data, SceneMetadata& sceneMetadata)
	{
		for (size_t sceneIdx = 0; sceneIdx < data->scenes_count; ++sceneIdx)
		{
			// Create our scene node entity hierarchy with a DFS traversal starting from each root node of the GLTF scene
			std::stack<cgltf_node const*> nodes;
			for (size_t nodeIdx = 0; nodeIdx < data->scenes[sceneIdx].nodes_count; ++nodeIdx)
			{
				if (data->scenes[sceneIdx].nodes[nodeIdx]->parent == nullptr)
				{
					nodes.emplace(data->scenes[sceneIdx].nodes[nodeIdx]);
				}
			}

			while (!nodes.empty())
			{
				cgltf_node const* curNode = nodes.top();
				nodes.pop();

				// Get our parent entity:
				entt::entity curNodeParentEntity = entt::null;
				if (curNode->parent)
				{
					SEAssert(sceneMetadata.m_nodeToEntity.contains(curNode->parent),
						"Failed to find the parent, this should not be possible");

					curNodeParentEntity = sceneMetadata.m_nodeToEntity.at(curNode->parent);
				}

				// Create the current node's entity (and Transform, if it has one):
				sceneMetadata.m_nodeToEntity.emplace(curNode, CreateSceneNode(curNode, curNodeParentEntity));

				// Add the children:
				for (size_t childIdx = 0; childIdx < curNode->children_count; ++childIdx)
				{
					nodes.emplace(curNode->children[childIdx]);
				}
			}
		}
	}


	void LoadAnimationData(
		std::string const& sceneFilePath, cgltf_data const* data, SceneMetadata& sceneMetadata)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		sceneMetadata.m_animationController = 
			fr::AnimationController::CreateAnimationController(em, sceneFilePath.c_str());

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

			sceneMetadata.m_animationController->AddNewAnimation(animationName.c_str());

			// Pack the Channels of an AnimationData struct:
			std::unordered_map<cgltf_node const*, fr::AnimationData>& nodeToData = 
				sceneMetadata.m_nodeToAnimationData.emplace_back();
			for (uint64_t channelIdx = 0; channelIdx < data->animations[animIdx].channels_count; ++channelIdx)
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

				animationData->m_animationIdx = animIdx;

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

				animChannel.m_keyframeTimesIdx = 
					sceneMetadata.m_animationController->AddKeyframeTimes(std::move(keyframeTimesSec));

				// Channel output data:
				const cgltf_size numOutputFloats = cgltf_accessor_unpack_floats(animSampler->output, nullptr, 0);

				std::vector<float> outputFloatData(numOutputFloats);
				cgltf_accessor_unpack_floats(animSampler->output, outputFloatData.data(), numOutputFloats);

				animChannel.m_dataIdx = sceneMetadata.m_animationController->AddChannelData(std::move(outputFloatData));

				SEAssert(numOutputFloats % numKeyframeTimeEntries == 0,
					"The number of keyframe entries must be an exact multiple of the number of output floats");

				animChannel.m_dataFloatsPerKeyframe = util::CheckedCast<uint8_t>(numOutputFloats / numKeyframeTimeEntries);
			}
		}
	}


	// Note: data must already be populated by calling cgltf_load_buffers
	void LoadSceneHierarchy(
		std::string const& sceneFilePath, std::string const& sceneRootPath, re::SceneData& sceneData, cgltf_data* data)
	{
		LOG("Scene has %d object nodes", data->nodes_count);

		// We'll pre-parse the scene and populate helper metadata
		SceneMetadata sceneMetadata;

		std::vector<std::future<void>> asyncLoadTasks;

		// Asyncronously pre-create the scene entity/transform hierarchy:
		asyncLoadTasks.emplace_back(
			core::ThreadPool::Get()->EnqueueJob([data, &sceneMetadata]() {
				CreateSceneNodeEntities(data, sceneMetadata);
				}));

		// Asyncronously pre-process the animation data:
		asyncLoadTasks.emplace_back(
			core::ThreadPool::Get()->EnqueueJob([&sceneFilePath, data, &sceneMetadata]() {
				LoadAnimationData(sceneFilePath, data, sceneMetadata);
				}));

		// Wait for the async load tasks to be done:
		for (size_t taskIdx = 0; taskIdx < asyncLoadTasks.size(); ++taskIdx)
		{
			asyncLoadTasks[taskIdx].wait();
		}
		asyncLoadTasks.clear();

		// Asyncronously attach the components to the entities, now that they exist:
		AttachNodeComponents(sceneData, data, sceneMetadata, asyncLoadTasks);

		// Wait for all of the tasks to be done:
		for (size_t taskIdx = 0; taskIdx < asyncLoadTasks.size(); ++taskIdx)
		{
			asyncLoadTasks[taskIdx].wait();
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
		: m_sceneRenderSystemNameHash(k_sceneRenderSystemName)
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
		
		// Load the IBL/skybox HDRI:
		std::string sceneRootPath;
		core::Config::Get()->TryGetValue<std::string>("sceneRootPath", sceneRootPath);

		earlyLoadTasks.emplace_back(
			core::ThreadPool::Get()->EnqueueJob([&sceneData, &sceneRootPath]() {
				LoadIBLTexture(sceneRootPath, *sceneData);
				}));

		// Start loading the GLTF file data:
		bool foundCamera = false;
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

			// Load textures via materials, as they allow us to infer the format and color space of the image data:
			PreLoadMaterials(sceneRootPath, *sceneData, data);

			// Load the scene hierarchy:
			LoadSceneHierarchy(sceneFilePath, sceneRootPath, *sceneData, data);

			SEAssert(data->cameras_count > 0 || data->cameras == nullptr, "Camera pointer and count mismatch");
			foundCamera = data->cameras_count > 0;

			// Cleanup:
			cgltf_free(data);
		}

		if (!foundCamera) // Add a default camera:
		{
			earlyLoadTasks.emplace_back(
				core::ThreadPool::Get()->EnqueueJob([&sceneData]() {
					LoadAddCamera(*sceneData, entt::null, nullptr);
					}));
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