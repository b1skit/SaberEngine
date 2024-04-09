// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "AssetLoadUtils.h"
#include "CameraComponent.h"
#include "Config.h"
#include "CoreEngine.h"
#include "EntityCommands.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MaterialInstanceComponent.h"
#include "Material_GLTF.h"
#include "MeshConcept.h"
#include "MeshPrimitiveComponent.h"
#include "Buffer.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneNodeConcept.h"
#include "Shader.h"
#include "ShadowMap.h"
#include "Texture.h"
#include "ThreadPool.h"
#include "ThreadSafeVector.h"
#include "Transform.h"
#include "VertexStreamBuilder.h"

#pragma warning(disable : 4996) // Suppress error C4996 (Caused by use of fopen, strcpy, strncpy in cgltf.h)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"


namespace
{
	std::shared_ptr<re::Texture> LoadTextureOrColor(
		fr::SceneData& scene,
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
					const std::string texNameStr = util::GenerateEmbeddedTextureName(texture->image->name);
					if (scene.TextureExists(texNameStr))
					{
						tex = scene.GetTexture(texNameStr);
					}
					else
					{
						tex = util::LoadTextureFromMemory(
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
					tex = util::LoadTextureFromFilePath(
						{ texNameStr }, colorSpace, false, re::Texture::k_errorTextureColor);
				}
			}
			else if (texture->image->buffer_view) // texture data is already loaded in memory
			{
				const std::string texNameStr = util::GenerateEmbeddedTextureName(texture->image->name);

				if (scene.TextureExists(texNameStr))
				{
					tex = scene.GetTexture(texNameStr);
				}
				else
				{
					unsigned char const* texSrc = static_cast<unsigned char const*>(
						texture->image->buffer_view->buffer->data) + texture->image->buffer_view->offset;

					const uint32_t texSrcNumBytes = static_cast<uint32_t>(texture->image->buffer_view->size);
					tex = util::LoadTextureFromMemory(texNameStr, texSrc, texSrcNumBytes, colorSpace);
				}
			}

			SEAssert(tex != nullptr, "Failed to load texture: Does the asset exist?");
		}
		else
		{
			// Create a texture color fallback:
			re::Texture::TextureParams colorTexParams
			{
				.m_usage = re::Texture::Usage::Color,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = formatFallback,
				.m_colorSpace = colorSpace
			};

			const size_t numChannels = re::Texture::GetNumberOfChannels(formatFallback);
			const std::string fallbackName = util::GenerateTextureColorFallbackName(colorFallback, numChannels, colorSpace);

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


	void GenerateErrorMaterial(fr::SceneData& scene)
	{
		LOG("Generating an error material \"%s\"...", fr::SceneData::k_missingMaterialName);

		constexpr char const* k_missingAlbedoTexName			= "MissingAlbedoTexture";
		constexpr char const* k_missingMetallicRoughnessTexName	= "MissingMetallicRoughnessTexture";
		constexpr char const* k_missingNormalTexName			= "MissingNormalTexture";
		constexpr char const* k_missingOcclusionTexName			= "MissingOcclusionTexture";
		constexpr char const* k_missingEmissiveTexName			= "MissingEmissiveTexture";

		std::shared_ptr<gr::Material> errorMat = gr::Material::Create(
			fr::SceneData::k_missingMaterialName, 
			gr::Material::MaterialType::GLTF_PBRMetallicRoughness);

		// MatAlbedo
		std::shared_ptr<re::Texture> errorAlbedo = util::LoadTextureFromFilePath(
			{ k_missingAlbedoTexName }, re::Texture::ColorSpace::sRGB, true, re::Texture::k_errorTextureColor);
		errorMat->SetTexture(0, errorAlbedo);

		// MatMetallicRoughness
		std::shared_ptr<re::Texture> errorMetallicRoughness = util::LoadTextureFromFilePath(
			{ k_missingMetallicRoughnessTexName }, re::Texture::ColorSpace::Linear, true, glm::vec4(0.f, 1.f, 0.f, 0.f));
		errorMat->SetTexture(1, errorMetallicRoughness);

		// MatNormal
		std::shared_ptr<re::Texture> errorNormal = util::LoadTextureFromFilePath(
			{ k_missingNormalTexName }, re::Texture::ColorSpace::Linear, true, glm::vec4(0.5f, 0.5f, 1.0f, 0.0f));
		errorMat->SetTexture(2, errorNormal);

		// MatOcclusion
		std::shared_ptr<re::Texture> errorOcclusion = util::LoadTextureFromFilePath(
			{ k_missingOcclusionTexName }, re::Texture::ColorSpace::Linear, true, glm::vec4(1.f));
		errorMat->SetTexture(3, errorOcclusion);

		// MatEmissive
		std::shared_ptr<re::Texture> errorEmissive = util::LoadTextureFromFilePath(
			{ k_missingEmissiveTexName }, re::Texture::ColorSpace::sRGB, true, re::Texture::k_errorTextureColor);
		errorMat->SetTexture(4, errorEmissive);

		scene.AddUniqueMaterial(errorMat);
	}


	void PreLoadMaterials(std::string const& sceneRootPath, fr::SceneData& scene, cgltf_data* data)
	{
		const size_t numMaterials = data->materials_count;
		LOG("Loading %d scene materials", numMaterials);

		// We assign each material to a thread; These threads will spawn new threads to load each texture. We need to 
		// wait on the future of each material to know when we can begin waiting on the futures for its textures
		std::vector<std::future<util::ThreadSafeVector<std::future<void>>>> matFutures;
		matFutures.reserve(numMaterials);

		for (size_t cur = 0; cur < numMaterials; cur++)
		{
			matFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob([data, cur, &scene, &sceneRootPath]() 
					-> util::ThreadSafeVector<std::future<void>>
				{

					util::ThreadSafeVector<std::future<void>> textureFutures;
					textureFutures.reserve(5); // Albedo, met/rough, normal, occlusion, emissive

					cgltf_material const* const material = &data->materials[cur];

					const std::string matName = 
						material == nullptr ? "MissingMaterial" : util::GenerateMaterialName(*material);
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
						gr::Material::Create(matName, gr::Material::MaterialType::GLTF_PBRMetallicRoughness);

					// GLTF specifications: If a texture is not given, all texture components are assumed to be 1.f
					// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
					constexpr glm::vec4 missingTextureColor(1.f, 1.f, 1.f, 1.f);

					// MatAlbedo
					textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
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
					textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
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
					textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
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
					textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
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
					textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
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
	void LoadAddCamera(fr::SceneData& scene, entt::entity sceneNode, cgltf_node* current)
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
			camConfig.m_aspectRatio = en::Config::Get()->GetWindowAspectRatio();
			camConfig.m_yFOV = en::Config::Get()->GetValue<float>("defaultyFOV");
			camConfig.m_near = en::Config::Get()->GetValue<float>("defaultNear");
			camConfig.m_far = en::Config::Get()->GetValue<float>("defaultFar");

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


	void LoadAddLight(fr::SceneData& scene, cgltf_node* current, entt::entity sceneNode)
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


	void LoadIBLTexture(std::string const& sceneRootPath, fr::SceneData& scene)
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
				iblTexture = util::LoadTextureFromFilePath(
					{ IBLPath },
					re::Texture::ColorSpace::Linear,
					false, 
					re::Texture::k_errorTextureColor);
			}
		};

		std::string IBLPath;
		if (en::Config::Get()->TryGetValue<std::string>(en::ConfigKeys::k_sceneIBLPathKey, IBLPath))
		{
			TryLoadIBL(IBLPath, iblTexture);
		}		
		
		if (!iblTexture)
		{
			IBLPath = en::Config::Get()->GetValue<std::string>(en::ConfigKeys::k_defaultEngineIBLPathKey);
			TryLoadIBL(IBLPath, iblTexture);
		}
		SEAssert(iblTexture != nullptr,
			std::format("Missing IBL texture. Per scene IBLs must be placed at {}; A default fallback must exist at {}",
				en::Config::Get()->GetValueAsString(en::ConfigKeys::k_sceneIBLPathKey),
				en::Config::Get()->GetValueAsString(en::ConfigKeys::k_defaultEngineIBLPathKey)).c_str());
	}


	void LoadMeshGeometry(
		std::string const& sceneRootPath, fr::SceneData& scene, cgltf_node* current, entt::entity sceneNode)
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
			util::VertexStreamBuilder::MeshData meshData
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
			util::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

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
					util::GenerateMaterialName(*current->mesh->primitives[primitive].material);
				material = scene.GetMaterial(generatedMatName);
			}
			else
			{
				LOG_WARNING("MeshPrimitive \"%s\" does not have a material. Assigning \"%s\"", 
					meshName.c_str(), fr::SceneData::k_missingMaterialName);
				material = scene.GetMaterial(fr::SceneData::k_missingMaterialName);
			}
			fr::MaterialInstanceComponent::AttachMaterialComponent(em, meshPrimimitiveEntity, material.get());
		}
	}


	void LoadObjectHierarchyRecursiveHelper(
		std::string const& sceneRootPath,
		fr::SceneData& scene,
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
		loadTasks.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob([current, parentSceneNode]()
		{
			SetTransformValues(current, parentSceneNode);
		}));
		
		// Process node attachments:
		if (current->mesh)
		{
			loadTasks.emplace_back(
				en::CoreEngine::GetThreadPool()->EnqueueJob([&sceneRootPath, &scene, current, parentSceneNode]()
			{
				LoadMeshGeometry(sceneRootPath, scene, current, parentSceneNode);
			}));
		}
		if (current->light)
		{
			loadTasks.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob([&scene, current, parentSceneNode]()
			{
				LoadAddLight(scene, current, parentSceneNode);
			}));
		}
		if (current->camera)
		{
			loadTasks.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob([&scene, current, parentSceneNode]()
			{
				LoadAddCamera(scene, parentSceneNode, current);
			}));
		}
	}


	// Note: data must already be populated by calling cgltf_load_buffers
	void LoadSceneHierarchy(std::string const& sceneRootPath, fr::SceneData& scene, cgltf_data* data)
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



/**********************************************************************************************************************
* SceneData members:
***********************************************************************************************************************/

namespace fr
{
	bool SceneData::Load(std::string const& sceneFilePath)
	{
		//stbi_set_flip_vertically_on_load(false); // Set this once. Note: It is NOT thread safe, and must be consistent

		constexpr size_t k_minReserveAmt = 10;
		size_t nodesCount = k_minReserveAmt;
		size_t meshesCount = k_minReserveAmt;
		size_t texturesCount = k_minReserveAmt;
		size_t materialsCount = k_minReserveAmt;
		size_t lightsCount = k_minReserveAmt;
		size_t camerasCount = k_minReserveAmt;

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
			nodesCount = static_cast<size_t>(data->nodes_count);
			meshesCount = static_cast<size_t>(data->meshes_count);
			texturesCount = static_cast<size_t>(data->textures_count);
			materialsCount = static_cast<size_t>(data->materials_count);
			lightsCount = static_cast<size_t>(data->lights_count);
			camerasCount = static_cast<size_t>(data->cameras_count);
		}

		// Pre-reserve our vectors, now that we know what to expect:
		m_textures.reserve(std::max(texturesCount, k_minReserveAmt));
		m_materials.reserve(std::max(materialsCount, k_minReserveAmt));

		std::string sceneRootPath;
		en::Config::Get()->TryGetValue<std::string>("sceneRootPath", sceneRootPath);

		std::vector<std::future<void>> earlyLoadTasks;

		earlyLoadTasks.emplace_back( 
			en::CoreEngine::GetThreadPool()->EnqueueJob([this]() {
				GenerateErrorMaterial(*this);
			}));

		// Add a default camera to start:
		earlyLoadTasks.emplace_back(
			en::CoreEngine::GetThreadPool()->EnqueueJob([this]() {
				LoadAddCamera(*this, entt::null, nullptr);
			}));

		// Load the IBL/skybox HDRI:
		earlyLoadTasks.emplace_back(
			en::CoreEngine::GetThreadPool()->EnqueueJob([this, &sceneRootPath]() {
				LoadIBLTexture(sceneRootPath, *this);
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
			PreLoadMaterials(sceneRootPath, *this, data);

			// Load the scene hierarchy:
			LoadSceneHierarchy(sceneRootPath, *this, data);

			// Cleanup:
			cgltf_free(data);
		}
		
		// Wait for all of the tasks we spawned here to be done:
		for (size_t loadTask = 0; loadTask < earlyLoadTasks.size(); loadTask++)
		{
			earlyLoadTasks[loadTask].wait();
		}

		m_isCreated = true;

		return true;
	}


	SceneData::SceneData(std::string const& sceneName)
		: NamedObject(sceneName)
		, m_isCreated(false)
	{
	}


	SceneData::~SceneData()
	{
		SEAssert(m_isCreated == false, "Did the SceneData go out of scope before Destroy was called?");
	}


	void SceneData::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_meshPrimitivesMutex);
			m_meshPrimitives.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_vertexStreamsMutex);
			m_vertexStreams.clear();
		}
		{
			std::unique_lock<std::shared_mutex> writeLock(m_texturesReadWriteMutex);
			m_textures.clear();
		}
		{
			std::unique_lock<std::shared_mutex> writeLock(m_materialsReadWriteMutex);
			m_materials.clear();
		}
		{
			std::unique_lock<std::shared_mutex> writeLock(m_shadersReadWriteMutex);
			m_shaders.clear();
		}

		m_isCreated = false; // Flag that Destroy has been called
	}


	re::Texture const* SceneData::GetIBLTexture() const
	{
		// We search for a scene-specific IBL, and fallback to the engine default IBL if it's not found
		std::shared_ptr<re::Texture> iblTexture = nullptr;
		std::string sceneIBLPath;
		bool result = en::Config::Get()->TryGetValue<std::string>(en::ConfigKeys::k_sceneIBLPathKey, sceneIBLPath);
		if (result)
		{
			iblTexture = TryGetTexture(sceneIBLPath);
		}
		
		if (!iblTexture)
		{
			std::string const& defaultIBLPath = 
				en::Config::Get()->GetValue<std::string>(en::ConfigKeys::k_defaultEngineIBLPathKey);
			iblTexture = GetTexture(defaultIBLPath); // Guaranteed to exist
		}

		return iblTexture.get();
	}


	bool SceneData::AddUniqueMeshPrimitive(std::shared_ptr<gr::MeshPrimitive>& meshPrimitive)
	{
		const uint64_t meshPrimitiveDataHash = meshPrimitive->GetDataHash();
		bool replacedIncomingPtr = false;
		{
			std::lock_guard<std::mutex> lock(m_meshPrimitivesMutex);

			auto const& result = m_meshPrimitives.find(meshPrimitiveDataHash);
			if (result != m_meshPrimitives.end())
			{
				LOG("MeshPrimitive \"%s\" has the same data hash as an existing MeshPrimitive. It will be replaced "
					"with a shared copy",
					meshPrimitive->GetName().c_str());

				meshPrimitive = result->second;
				replacedIncomingPtr = true;

				//// Add a marker to simplify debugging of shared meshes
				//constexpr char const* k_sharedMeshTag = " <shared>";
				//if (meshPrimitive->GetName().find(k_sharedMeshTag) == std::string::npos)
				//{
				//	meshPrimitive->SetName(meshPrimitive->GetName() + k_sharedMeshTag);
				//}
				
				// BUG HERE: We (currently) can't set the name on something that is shared, as another thread might be
				// using it (e.g. dereferencing .c_str())
			}
			else
			{
				m_meshPrimitives.insert({ meshPrimitiveDataHash, meshPrimitive });
			}
		}
		return replacedIncomingPtr;
	}


	bool SceneData::AddUniqueVertexStream(std::shared_ptr<re::VertexStream>& vertexStream)
	{
		const uint64_t vertexStreamDataHash = vertexStream->GetDataHash();
		bool replacedIncomingPtr = false;
		{
			std::lock_guard<std::mutex> lock(m_vertexStreamsMutex);

			auto const& result = m_vertexStreams.find(vertexStreamDataHash);
			if (result != m_vertexStreams.end())
			{
				LOG(std::format("Vertex stream has the same data hash \"{}\" as an existing vertex stream. It will be "
					"replaced with a shared copy", vertexStreamDataHash).c_str());

				vertexStream = result->second;
				replacedIncomingPtr = true;
			}
			else
			{
				m_vertexStreams.insert({ vertexStreamDataHash, vertexStream });
			}
		}
		return replacedIncomingPtr;
	}


	bool SceneData::AddUniqueTexture(std::shared_ptr<re::Texture>& newTexture)
	{
		SEAssert(newTexture != nullptr, "Cannot add null texture to textures table");

		{
			std::unique_lock<std::shared_mutex> writeLock(m_texturesReadWriteMutex);

			std::unordered_map<size_t, std::shared_ptr<re::Texture>>::const_iterator texturePosition =
				m_textures.find(newTexture->GetNameID());

			bool foundExisting = false;
			if (texturePosition != m_textures.end()) // Found existing
			{
				LOG("Texture \"%s\" has alredy been registed with scene", newTexture->GetName().c_str());
				newTexture = texturePosition->second;
				foundExisting = true;
			}
			else  // Add new
			{
				m_textures[newTexture->GetNameID()] = newTexture;
				LOG("Texture \"%s\" registered with scene", newTexture->GetName().c_str());
			}

			return foundExisting;
		}
	}


	std::shared_ptr<re::Texture> SceneData::GetTexture(std::string const& texName) const
	{
		const uint64_t nameID = en::NamedObject::ComputeIDFromName(texName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_texturesReadWriteMutex);

			auto result = m_textures.find(nameID);

			SEAssert(result != m_textures.end(), "Texture with that name does not exist");

			return result->second;
		}
	}


	std::shared_ptr<re::Texture> SceneData::TryGetTexture(std::string const& texName) const
	{
		const uint64_t nameID = en::NamedObject::ComputeIDFromName(texName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_texturesReadWriteMutex);

			auto result = m_textures.find(nameID);
			return result == m_textures.end() ? nullptr : result->second;
		}
	}


	bool SceneData::TextureExists(std::string const& textureName) const
	{
		const uint64_t nameID = en::NamedObject::ComputeIDFromName(textureName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_texturesReadWriteMutex);
			return m_textures.find(nameID) != m_textures.end();
		}
	}


	std::shared_ptr<re::Texture> SceneData::TryLoadUniqueTexture(
		std::string const& filepath, re::Texture::ColorSpace colorSpace)
	{
		std::shared_ptr<re::Texture> result = TryGetTexture(filepath);

		SEAssert(result == nullptr || result->GetTextureParams().m_colorSpace == colorSpace,
			"Found a texture with the same filepath name, but different colorspace. This is unexpected");

		if (result == nullptr)
		{
			result = util::LoadTextureFromFilePath({ filepath }, colorSpace, false);
			if (result)
			{
				AddUniqueTexture(result);
			}
		}

		return result;
	}


	void SceneData::AddUniqueMaterial(std::shared_ptr<gr::Material>& newMaterial)
	{
		SEAssert(newMaterial != nullptr, "Cannot add null material to material table");

		{
			std::unique_lock<std::shared_mutex> writeLock(m_materialsReadWriteMutex);

			// Note: Materials are uniquely identified by name, regardless of the MaterialDefinition they might use
			std::unordered_map<size_t, std::shared_ptr<gr::Material>>::const_iterator matPosition =
				m_materials.find(newMaterial->GetNameID());
			if (matPosition != m_materials.end()) // Found existing
			{
				newMaterial = matPosition->second;
			}
			else // Add new
			{
				m_materials[newMaterial->GetNameID()] = newMaterial;
				LOG("Material \"%s\" registered to scene data", newMaterial->GetName().c_str());
			}
		}
	}


	std::shared_ptr<gr::Material> SceneData::GetMaterial(std::string const& materialName) const
	{
		const size_t nameID = NamedObject::ComputeIDFromName(materialName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_materialsReadWriteMutex);
			std::unordered_map<size_t, std::shared_ptr<gr::Material>>::const_iterator matPos = m_materials.find(nameID);

			SEAssert(matPos != m_materials.end(), "Could not find material");

			return matPos->second;
		}
	}


	bool SceneData::MaterialExists(std::string const& matName) const
	{
		const size_t nameID = NamedObject::ComputeIDFromName(matName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_materialsReadWriteMutex);

			return m_materials.find(nameID) != m_materials.end();
		}
	}


	std::vector<std::string> SceneData::GetAllMaterialNames() const
	{
		std::vector<std::string> result;
		{
			std::shared_lock<std::shared_mutex> readLock(m_materialsReadWriteMutex);

			for (auto const& material : m_materials)
			{
				result.emplace_back(material.second->GetName());
			}
		}
		return result;
	}


	bool SceneData::AddUniqueShader(std::shared_ptr<re::Shader>& newShader)
	{
		SEAssert(newShader != nullptr, "Cannot add null shader to shader table");
		{
			std::unique_lock<std::shared_mutex> writeLock(m_shadersReadWriteMutex);

			bool addedNewShader = false;

			const uint64_t shaderIdentifier = newShader->GetShaderIdentifier();

			// Note: Materials are uniquely identified by name, regardless of the MaterialDefinition they might use
			std::unordered_map<size_t, std::shared_ptr<re::Shader>>::const_iterator shaderPosition =
				m_shaders.find(shaderIdentifier);
			if (shaderPosition != m_shaders.end()) // Found existing
			{
				newShader = shaderPosition->second;
				addedNewShader = false;
			}
			else // Add new
			{
				m_shaders[shaderIdentifier] = newShader;
				addedNewShader = true;
				LOG("Shader \"%s\" registered with scene", newShader->GetName().c_str());
			}
			return addedNewShader;
		}
	}


	std::shared_ptr<re::Shader> SceneData::GetShader(uint64_t shaderIdentifier) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_shadersReadWriteMutex);
			std::unordered_map<size_t, std::shared_ptr<re::Shader>>::const_iterator shaderPos = m_shaders.find(shaderIdentifier);

			SEAssert(shaderPos != m_shaders.end(), "Could not find shader");

			return shaderPos->second;
		}
	}


	bool SceneData::ShaderExists(uint64_t shaderIdentifier) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_shadersReadWriteMutex);

			return m_materials.find(shaderIdentifier) != m_materials.end();
		}
	}
}