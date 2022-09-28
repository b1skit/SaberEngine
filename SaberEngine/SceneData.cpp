#include <memory>
#include <vector>
#include <sstream>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION	// Only include this define ONCE in the project
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#pragma warning(disable : 4996) // Suppress error C4996 (Caused by use of fopen, strcpy, strncpy in cgltf.h)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"


#include "SceneData.h"
#include "TangentBuilder.h"
#include "Light.h"
#include "Camera.h"
#include "GameObject.h"
#include "RenderMesh.h"
#include "Mesh.h"
#include "Transform.h"
#include "Material.h"
#include "Light.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "ShadowMap.h"

using gr::Camera;
using gr::Light;
using gr::Texture;
using gr::Material;
using gr::Mesh;
using gr::Bounds;
using gr::Transform;
using gr::Light;
using gr::ShadowMap;
using fr::GameObject;
using en::CoreEngine;
using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::unordered_map;
using std::stringstream;
using std::max;
using glm::quat;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4;
using glm::make_mat4;
using glm::decompose;


// Data loading helpers:
namespace
{
	#define ERROR_TEXTURE_NAME "ErrorTexture"
	#define ERROR_TEXTURE_COLOR_VEC4 vec4(1.0f, 0.0f, 1.0f, 1.0f)
	#define DEFAULT_ALPHA_VALUE 1.0f // Default alpha value when loading texture data, if no alpha exists


	// Helper functions for loading Low/High Dynamic Range image formats in LoadTextureFileFromPath()
	// Note: targetTexture and imageData must be valid
	void LoadLDRHelper(
		std::vector<glm::vec4>& texels,
		const uint8_t* imageData,
		size_t width,
		size_t height,
		size_t numChannels,
		size_t firstTexelIndex = 0)
	{
		// Read texel values:
		const uint8_t* currentElement = imageData;
		for (size_t row = 0; row < height; row++)
		{
			for (size_t col = 0; col < width; col++)
			{
				vec4 currentPixel(0.0f, 0.0f, 0.0f, DEFAULT_ALPHA_VALUE);

				for (size_t channel = 0; channel < numChannels; channel++)
				{
					// LDR values are stored as normalized floats
					currentPixel[(uint32_t)channel] = (float)((float)((unsigned int)*currentElement) / 255.0f);
					currentElement++;
				}

				texels.at(firstTexelIndex + (row * width) + col) = currentPixel;
			}
		}
	}


	void LoadHDRHelper(
		std::vector<glm::vec4>& texels,
		float const* imageData,
		size_t width, size_t height,
		size_t numChannels,
		size_t firstTexelIndex = 0)
	{
		// Typically most HDRs will be RGB, with 32-bits per channel (https://www.hdrsoft.com/resources/dri.html)

		// Read texel values:
		const float* currentElement = imageData;
		for (size_t row = 0; row < height; row++)
		{
			for (size_t col = 0; col < width; col++)
			{
				// Start with an "empty" pixel, and fill values as we encounter them
				vec4 currentPixel(0.0f, 0.0f, 0.0f, DEFAULT_ALPHA_VALUE);

				// TODO: Support RBG formats for HDR images: We can copy the image data directly into our m_texels array

				for (size_t channel = 0; channel < numChannels; channel++)
				{
					currentPixel[(uint32_t)channel] = *currentElement;

					currentElement++;
				}

				texels.at(firstTexelIndex + (row * width) + col) = currentPixel;
			}
		}
	}


	std::shared_ptr<gr::Texture> LoadTextureFileFromPath(vector<string> texturePaths, bool returnErrorTex = false)
	{
		SEAssert("Can load single faces or cubemaps only", texturePaths.size() == 1 || texturePaths.size() == 6);
		SEAssert("Invalid number of texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);

		LOG("Attempting to load %d textures: \"%s\"...", texturePaths.size(), texturePaths[0].c_str());

		// Flip the y-axis on loading (so pixel (0,0) is in the bottom-left of the image if using OpenGL
		platform::RenderingAPI const& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();
		const bool flipY = api == platform::RenderingAPI::OpenGL ? true : false;

		stbi_set_flip_vertically_on_load(flipY);

		const uint32_t totalFaces = (uint32_t)texturePaths.size();

		// Start with parameters suitable for an error texture:
		Texture::TextureParams texParams;
		texParams.m_width = 2;
		texParams.m_height = 2;
		texParams.m_faces = totalFaces;

		texParams.m_texUse = gr::Texture::TextureUse::Color;
		texParams.m_texDimension = totalFaces == 1 ?
			gr::Texture::TextureDimension::Texture2D : gr::Texture::TextureDimension::TextureCubeMap;
		texParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
		texParams.m_texColorSpace = Texture::TextureColorSpace::Unknown;

		texParams.m_clearColor = ERROR_TEXTURE_COLOR_VEC4;
		texParams.m_texturePath = ERROR_TEXTURE_NAME;
		texParams.m_useMIPs = true;

		// Load the texture, face-by-face:
		shared_ptr<Texture> texture(nullptr);
		for (size_t face = 0; face < totalFaces; face++)
		{
			// Get the image data:
			int width, height, numChannels;
			void* imageData = nullptr;
			size_t bitDepth = 0;

			if (stbi_is_hdr(texturePaths[face].c_str()))	// HDR
			{
				imageData = stbi_loadf(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 32;
			}
			else if (stbi_is_16_bit(texturePaths[face].c_str()))
			{
				// TODO: Support loading 16 bit images
				LOG_WARNING("Loading 16 bit image as 8 bit");
				imageData = stbi_load(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 8;
				//imageData = stbi_load_16(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				//bitDepth = 16;
			}
			else // Non-HDR
			{
				imageData = stbi_load(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 8;
			}

			if (imageData)
			{
				LOG("Found %dx%d, %d-bit texture with %d channels", width, height, bitDepth, numChannels);

				if (texture == nullptr)
				{
					// Update the texture parameters:
					texParams.m_width = width;
					texParams.m_height = height;
					texParams.m_faces = (uint32_t)totalFaces;

					if ((width == 1 || height == 1) && (width != height))
					{
						LOG_WARNING("Found 1D texture, but 1D textures are currently not supported. Treating "
							"this texture as 2D");
						texParams.m_texDimension = gr::Texture::TextureDimension::Texture2D; // TODO: Support 1D textures
						/*texParams.m_texDimension = gr::Texture::TextureDimension::Texture1D;*/
					}

					// Currently, we force-pack everything into a 4-channel, 32-bit RGBA texture (in our LDR/HDR helpers).
					// TODO: Support arbitrary texture layouts
					texParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F;

					texParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color
					texParams.m_texturePath = texturePaths[0]; // Note: Texture lookup also uses the path of the first face

					texture = std::make_shared<gr::Texture>(texParams);
				}
				else
				{
					SEAssert("Parameter mismatch", texParams.m_width == width && texParams.m_height == height);
				}

				const size_t firstTexelIndex = face * width * height;
				if (bitDepth == 32)
				{
					float const* castImageData = static_cast<float const*>(imageData);

					LoadHDRHelper(texture->Texels(), castImageData, width, height, numChannels, firstTexelIndex);
				}
				else if (bitDepth == 8 || bitDepth == 16)
				{
					uint8_t const* castImageData = static_cast<uint8_t const*>(imageData);
					LoadLDRHelper(texture->Texels(), castImageData, width, height, numChannels, firstTexelIndex);
				}
				else
				{
					SEAssert("Invalid bit depth", false);
				}

				// Cleanup:
				stbi_image_free(imageData);
			}
			else if (returnErrorTex)
			{
				if (texture != nullptr)
				{
					// TODO...
					SEAssert("TODO: Cleanup existing texture, reset texParams to be suitable for an error texture", false);
				}
				texture = std::make_shared<gr::Texture>(texParams);
			}
			else
			{
				char const* failResult = stbi_failure_reason();
				LOG_WARNING("Failed to load image \"%s\": %s", texturePaths[0].c_str(), failResult);
				return nullptr;
			}
		}

		// Note: Texture color space must be set, and Create() must be called
		return texture; 
	}




	// GLTF loading helpers:
	/*****************************************************************************************************************/



	// Generate a unique name for a material from (some of) the values in the cgltf_material struct
	string GenerateMaterialName(cgltf_material const& material)
	{
		if (material.name)
		{
			return string(material.name);
		}
		SEAssert("Specular/Glossiness materials are not currently supported", material.has_pbr_specular_glossiness == 0);

		// TODO: Expand the values used to generate the name here, and/or use hashes to identify materials
		// -> String streams are very slow...
		stringstream matName;

		matName << material.pbr_metallic_roughness.base_color_texture.texture;
		matName << material.pbr_metallic_roughness.metallic_roughness_texture.texture;

		matName << material.pbr_metallic_roughness.base_color_factor[0]
			<< material.pbr_metallic_roughness.base_color_factor[1]
			<< material.pbr_metallic_roughness.base_color_factor[2]
			<< material.pbr_metallic_roughness.base_color_factor[3];

		matName << material.pbr_metallic_roughness.metallic_factor;
		matName << material.pbr_metallic_roughness.roughness_factor;

		matName << material.emissive_strength.emissive_strength;
		matName << material.normal_texture.texture;
		matName << material.occlusion_texture.texture;
		matName << material.emissive_texture.texture;
		matName << material.emissive_factor[0] << material.emissive_factor[2] << material.emissive_factor[3];
		matName << material.alpha_mode;
		matName << material.alpha_cutoff;

		return matName.str();
	}


	shared_ptr<Material> LoadAddMaterial(
		fr::SceneData& scene, std::string const& rootPath, cgltf_material const* material)
	{
		if (material == nullptr)
		{
			LOG_ERROR("Mesh does not have a material. Loading an error material");
			shared_ptr<Material> newMat =
				make_shared<Material>("Missing material", Material::GetMaterialDefinition("pbrMetallicRoughness"));
		
			for (size_t i = 0; i < 5; i++)
			{
				newMat->GetTexture((uint32_t)i) = scene.GetLoadTextureByPath({ ERROR_TEXTURE_NAME }, true);
			}
			return newMat;
		}
		SEAssert("Unsupported material model", material->has_pbr_metallic_roughness == 1);

		const string matName = GenerateMaterialName(*material);
		if (scene.MaterialExists(matName))
		{
			return scene.GetMaterial(matName);
		}

		shared_ptr<Material> newMat =
			make_shared<Material>(matName, Material::GetMaterialDefinition("pbrMetallicRoughness"));

		newMat->GetShader() = nullptr; // Not required; just for clarity
	
		newMat->Property(Material::MATERIAL_PROPERTY_INDEX::MatProperty0) = vec4(0.04f, 0.04f, 0.04f, 1.0f);
		
		auto LoadTextureOrColor = [&](
			cgltf_texture* texture, 
			vec4 const& colorFallback, 
			Texture::TextureFormat format, 
			Texture::TextureColorSpace colorSpace)
		{
			// TODO: Support arbitary channel formats beyond 4-channel RGBA

			shared_ptr<Texture> tex;
			if (texture && texture->image && texture->image->uri)
			{
				tex = scene.GetLoadTextureByPath(
					{ rootPath + texture->image->uri });

				Texture::TextureParams texParams = tex->GetTextureParams();
				texParams.m_texColorSpace = colorSpace;
				texParams.m_texFormat = format;
				tex->SetTextureParams(texParams);
			}
			else
			{
				Texture::TextureParams colorTexParams;
				colorTexParams.m_clearColor = colorFallback; // Clear color = initial fill color
				colorTexParams.m_texturePath = "Color_" +
					to_string(colorTexParams.m_clearColor.x) + "_" +
					to_string(colorTexParams.m_clearColor.y) + "_" +
					to_string(colorTexParams.m_clearColor.z) + "_" +
					to_string(colorTexParams.m_clearColor.w) + "_" +
					(colorSpace == Texture::TextureColorSpace::sRGB ? "sRGB" : "Linear");
				colorTexParams.m_texColorSpace = colorSpace;
				colorTexParams.m_texFormat = format;
				tex = make_shared<Texture>(colorTexParams);
			}

			scene.AddUniqueTexture(tex);
			tex->Create(); // Create the texture after calling AddUniqueTexture(), as we now know it won't be destroyed
			return tex;
		};

		// GLTF specifications: If a texture is not given, all respective texture components must be assumed to be 1.0f
		// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
		const vec4 missingTextureColor(1.0f, 1.0f, 1.0f, 1.0f);

		// MatAlbedo
		newMat->GetTexture(0) = LoadTextureOrColor(
			material->pbr_metallic_roughness.base_color_texture.texture,
			missingTextureColor,
			Texture::TextureFormat::RGBA8,
			Texture::TextureColorSpace::sRGB);

		// MatMetallicRoughness
		newMat->GetTexture(1) = LoadTextureOrColor(
			material->pbr_metallic_roughness.metallic_roughness_texture.texture,
			vec4(1.0f,
				material->pbr_metallic_roughness.roughness_factor,
				material->pbr_metallic_roughness.metallic_factor,
				1.0f),
			Texture::TextureFormat::RGBA8,
			Texture::TextureColorSpace::Linear);

		// MatNormal
		newMat->GetTexture(2) = LoadTextureOrColor(
			material->normal_texture.texture,
			vec4(0.5f, 0.5f, 1.0f, 0.0f), // Equivalent to a [0,0,1] normal after unpacking
			Texture::TextureFormat::RGBA32F,
			Texture::TextureColorSpace::Linear);

		// MatOcclusion
		newMat->GetTexture(3) = LoadTextureOrColor(
			material->occlusion_texture.texture,
			missingTextureColor,	// Completely unoccluded
			Texture::TextureFormat::RGBA32F,
			Texture::TextureColorSpace::Linear);

		// MatEmissive
		newMat->GetTexture(4) = LoadTextureOrColor(
			material->emissive_texture.texture,
			vec4(material->emissive_factor[0], material->emissive_factor[1], material->emissive_factor[2], 1.0f),
			Texture::TextureFormat::RGBA32F,
			Texture::TextureColorSpace::sRGB); // GLTF convention: Must be converted to linear before use

		// TODO: Support scaling factors (baseColorFactor, occlusion strength, emissive_factor/emissive_strength etc)
		// -> Multiply in with the missingTextureColor?

		scene.AddUniqueMaterial(newMat);
		return newMat;
	}


	// Creates a default camera if camera == nullptr, and no cameras exist in scene
	void LoadAddCamera(fr::SceneData& scene, shared_ptr<GameObject> parent, cgltf_camera* camera)
	{
		if (camera == nullptr && parent == nullptr)
		{
			if (scene.GetCameras().size() == 0) // Create a default camera at the origin
			{
				LOG("\nCreating a default camera");

				gr::Camera::CameraConfig camConfig;
				camConfig.m_aspectRatio = CoreEngine::GetCoreEngine()->GetConfig()->GetWindowAspectRatio();
				camConfig.m_fieldOfView = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultFieldOfView");
				camConfig.m_near = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultNear");
				camConfig.m_far = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultFar");
				camConfig.m_exposure = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultExposure");

				scene.AddCamera(make_shared<Camera>("Default camera", camConfig, nullptr));
			}

			return;
		}

		SEAssert("Must supply a parent and camera pointer", parent != nullptr && camera != nullptr);

		const string camName = camera->name ? string(camera->name) : "Unnamed camera";
		LOG("Loading camera \"%s\"", camName.c_str());

		gr::Camera::CameraConfig camConfig;
		camConfig.m_isOrthographic = camera->type == cgltf_camera_type_orthographic;
		if (camConfig.m_isOrthographic)
		{
			camConfig.m_fieldOfView = 0;
			camConfig.m_near		= camera->data.orthographic.znear;
			camConfig.m_far			= camera->data.orthographic.zfar;
			camConfig.m_orthoLeft	= -camera->data.orthographic.xmag / 2.0f;
			camConfig.m_orthoRight	= camera->data.orthographic.xmag / 2.0f;
			camConfig.m_orthoBottom = -camera->data.orthographic.ymag / 2.0f;
			camConfig.m_orthoTop	= camera->data.orthographic.ymag / 2.0f;
		}
		else
		{
			LOG_WARNING("Loading a perspective camera, but this implementation is not yet complete");

			// TODO: Store this in Radians, and convert to vertical FOV so we can use camera->data.perspective.yfov
			camConfig.m_fieldOfView = 90.0f;

			camConfig.m_near		= camera->data.perspective.znear;
			camConfig.m_far			= camera->data.perspective.zfar;
			camConfig.m_aspectRatio = camera->data.perspective.has_aspect_ratio ?
				camera->data.perspective.aspect_ratio : 1.0f;
			camConfig.m_orthoLeft	= 0;
			camConfig.m_orthoRight	= 0;
			camConfig.m_orthoBottom = 0;
			camConfig.m_orthoTop	= 0;
		}

		shared_ptr<Camera> newCam = make_shared<Camera>(camName, camConfig, nullptr);
		newCam->GetTransform()->SetParent(parent->GetTransform());
		scene.AddCamera(newCam);
	}


	void LoadAddLight(fr::SceneData& scene, shared_ptr<GameObject> parent, cgltf_light* light)
	{
		const string lightName = (light->name ? string(light->name) : "Unnamed light");

		LOG("Found light \"%s\"", lightName.c_str());

		Light::LightType lightType = Light::LightType::Directional;
		switch (light->type)
		{
		case cgltf_light_type::cgltf_light_type_directional:
		{
			lightType = Light::LightType::Directional;
		}
		break;
		case cgltf_light_type::cgltf_light_type_point:
		{
			lightType = Light::LightType::Point;
		}
		break;
		case cgltf_light_type::cgltf_light_type_spot:
		{
			LOG_WARNING("Found spot light type, but spotlights are not currently implemented. Ignoring!");
			return;
		}
		break;
		case cgltf_light_type::cgltf_light_type_invalid:
		case cgltf_light_type::cgltf_light_type_max_enum:
		default:
			SEAssert("Invalid light type", false);
		}

		const vec3 colorIntensity = glm::make_vec3(light->color) * light->intensity;

		shared_ptr<Light> newLight = 
			make_shared<Light>(lightName, parent->GetTransform(), lightType, colorIntensity, nullptr);

		scene.AddLight(newLight);
	}


	// Attach shadowmaps once the scene has been loaded so the scene Bounds are accurate
	void AttachShadowMaps(fr::SceneData& scene)
	{
		// TODO: Shadow setup should be handled in the Light constructor
		// Light should have an optional "hasShadow" flag
		// -> Keylight/Deferred light GS should check the bounds each frame instead of specifying shadow params once after scene load

		shared_ptr<Light> keylight = scene.GetKeyLight();
		if (keylight)
		{
			gr::Bounds sceneWorldBounds = scene.GetWorldSpaceSceneBounds();

			const gr::Bounds transformedBounds = sceneWorldBounds.GetTransformedBounds(
				glm::inverse(keylight->GetTransform()->GetWorldMatrix()));

			gr::Camera::CameraConfig shadowCamConfig;
			shadowCamConfig.m_near = -transformedBounds.zMax();
			shadowCamConfig.m_far = -transformedBounds.zMin();

			shadowCamConfig.m_isOrthographic = true;
			shadowCamConfig.m_orthoLeft = transformedBounds.xMin();
			shadowCamConfig.m_orthoRight = transformedBounds.xMax();
			shadowCamConfig.m_orthoBottom = transformedBounds.yMin();
			shadowCamConfig.m_orthoTop = transformedBounds.yMax();

			shared_ptr<ShadowMap> keyLightShadowMap = make_shared<ShadowMap>
			(
				keylight->GetName(),
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowMapWidth"),
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowMapHeight"),
				shadowCamConfig,
				keylight->GetTransform()
			);

			keylight->GetShadowMap() = keyLightShadowMap;
		}

		if (scene.GetPointLights().size() > 0)
		{
			gr::Camera::CameraConfig shadowCamConfig;
			shadowCamConfig.m_fieldOfView = 90.0f;
			shadowCamConfig.m_near = 1.0f;
			shadowCamConfig.m_aspectRatio = 1.0f;
			shadowCamConfig.m_isOrthographic = false;
			
			for (shared_ptr<Light> point : scene.GetPointLights())
			{
				shadowCamConfig.m_far = point->GetRadius();

				shared_ptr<ShadowMap> pointLightShadowMap = make_shared<ShadowMap>(
					point->GetName(),
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowCubeMapWidth"),
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowCubeMapHeight"), 
					shadowCamConfig,
					point->GetTransform(),
					vec3(0.0f, 0.0f, 0.0f),	// No offset
					true);
				
				pointLightShadowMap->MinShadowBias() =
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMinShadowBias");
				pointLightShadowMap->MaxShadowBias() = 
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMaxShadowBias");
				
				point->GetShadowMap() = pointLightShadowMap;
			}
		}
	}


	// Depth-first traversal
	void LoadObjectHierarchyRecursiveHelper(
		std::string const& rootPath, fr::SceneData& scene, cgltf_data* data, cgltf_node* current, shared_ptr<GameObject> parent)
	{
		if (current == nullptr)
		{
			return;
		}	

		const string nodeName = current->name ? string(current->name) : "unnamedNode";

		auto SetTransformValues = [](cgltf_node* current, Transform* targetTransform)
		{
			SEAssert("Transform has both matrix and decomposed properties", 
				(current->has_matrix != (current->has_rotation || current->has_scale || current->has_translation)) ||
					(current->has_matrix == 0 && current->has_rotation == 0 && 
					current->has_scale == 0 && current->has_translation == 0)
			);

			if (current->has_matrix)
			{
				const mat4 nodeModelMatrix = make_mat4(current->matrix);
				vec3 scale;
				quat rotation;
				vec3 translation;
				vec3 skew;
				vec4 perspective;
				decompose(nodeModelMatrix, scale, rotation, translation, skew, perspective);

				targetTransform->SetModelRotation(rotation);
				targetTransform->SetModelScale(scale);
				targetTransform->SetModelPosition(translation);
			}
			else
			{
				if (current->has_rotation)
				{
					// Note: GLM expects quaternions to be specified in WXYZ order
					targetTransform->SetModelRotation(
						glm::quat(current->rotation[3], current->rotation[0], current->rotation[1], current->rotation[2]));
				}
				if (current->has_scale)
				{
					targetTransform->SetModelScale(glm::vec3(current->scale[0], current->scale[1], current->scale[2]));
				}
				if (current->has_translation)
				{
					targetTransform->SetModelPosition(
						glm::vec3(current->translation[0], current->translation[1], current->translation[2]));
				}
			}
		};

		SEAssert("TODO: Handle nodes with multiple things that depend on a transform", 
			current->light == nullptr || current->mesh == nullptr);

		// Set the GameObject transform:
		if (current->mesh == nullptr)
		{
			SetTransformValues(current, parent->GetTransform());
		}
		else // Node has a mesh: Create a mesh primitive and attach it to a RenderMesh
		{
			// Add each Mesh primitive as a child of the GameObject's RenderMesh:
			for (size_t primitive = 0; primitive < current->mesh->primitives_count; primitive++)
			{
				SEAssert(
					"TODO: Support more primitive types/draw modes!",
					current->mesh->primitives[primitive].type == cgltf_primitive_type::cgltf_primitive_type_triangles);

				// Populate the mesh params:
				Mesh::MeshParams meshParams;
				switch (current->mesh->primitives[primitive].type)
				{
					case cgltf_primitive_type::cgltf_primitive_type_points:
					{
						meshParams.m_drawMode = Mesh::DrawMode::Points; 
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_lines:
					{
						meshParams.m_drawMode = Mesh::DrawMode::Lines;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_line_loop:
					{
						meshParams.m_drawMode = Mesh::DrawMode::LineLoop;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_line_strip:
					{
						meshParams.m_drawMode = Mesh::DrawMode::LineStrip;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_triangles:
					{
						meshParams.m_drawMode = Mesh::DrawMode::Triangles;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_triangle_strip:
					{
						meshParams.m_drawMode = Mesh::DrawMode::TriangleStrip;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_triangle_fan:
					{
						meshParams.m_drawMode = Mesh::DrawMode::TriangleFan;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_max_enum:
					default:
						SEAssert("Unsupported primitive type/draw mode", false);
				}

				SEAssert("Mesh is missing indices", current->mesh->primitives[primitive].indices != nullptr);
				vector<uint32_t> indices;
				indices.resize(current->mesh->primitives[primitive].indices->count, 0);
				for (size_t index = 0; index < current->mesh->primitives[primitive].indices->count; index++)
				{
					// Note: We use 32-bit indexes, but cgltf uses size_t's
					indices[index] = (uint32_t)cgltf_accessor_read_index(
						current->mesh->primitives[primitive].indices, (uint64_t)index);
				}

				// Unpack each of the primitive's vertex attrbutes:
				vector<float> positions;
				vector<float> normals;
				vector<float> colors;
				vector<float> uv0;
				vector<float> tangents;
				for (size_t attrib = 0; attrib < current->mesh->primitives[primitive].attributes_count; attrib++)
				{
					// TODO: Use the incoming pre-computed min/max to optimize local bounds calculation
					// -> Override the Mesh ctor!

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
							SEAssert("Invalid vertex attribute data type", false);
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
						uv0.resize(totalFloatElements, 0);
						dataTarget = &uv0[0];
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_color:
					{
						colors.resize(totalFloatElements, 0);
						dataTarget = &colors[0];
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_joints:
					case cgltf_attribute_type::cgltf_attribute_type_weights:
					case cgltf_attribute_type::cgltf_attribute_type_custom:
					case cgltf_attribute_type::cgltf_attribute_type_max_enum:
					case cgltf_attribute_type::cgltf_attribute_type_invalid:
					default:
						SEAssert("Invalid attribute type", false);
					}

					cgltf_accessor* const accessor = current->mesh->primitives[primitive].attributes[attrib].data;
					
					bool unpackResult = cgltf_accessor_unpack_floats(
						accessor,
						dataTarget,
						totalFloatElements);
					SEAssert("Failed to unpack data", unpackResult);

					// Post-process the data:
					if (attributeType == cgltf_attribute_type_texcoord)
					{
						// GLTF specifies (0,0) as the top-left of a texture. 
						// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#images
						// In OpenGL, we already flip the image Y onimport, so flip the UVs here to compensate
						platform::RenderingAPI const& api =
							en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();
						const bool flipY = api == platform::RenderingAPI::OpenGL ? true : false;
						if (flipY)
						{
							for (size_t v = 1; v < uv0.size(); v += 2)
							{
								uv0[v] = 1.0f - uv0[v];
							}
						}
					}
				} // End attribute unpacking

				// Construct any missing vertex attributes for the mesh:
				util::VertexAttributeBuilder::MeshData meshData
				{
					nodeName,
					&meshParams,
					&indices,
					reinterpret_cast<vector<vec3>*>(&positions),
					reinterpret_cast<vector<vec3>*>(&normals),
					reinterpret_cast<vector<vec2>*>(&uv0),
					reinterpret_cast<vector<vec4>*>(&tangents)
				};
				util::VertexAttributeBuilder tangentBuilder;
				tangentBuilder.ConstructMissingVertexAttributes(&meshData);

				// Material:
				shared_ptr<Material> material = 
					LoadAddMaterial(scene, rootPath, current->mesh->primitives[primitive].material);

				// Attach the primitive:
				parent->AddMeshPrimitive(make_shared<Mesh>(
					nodeName,
					positions,
					normals,
					colors,
					uv0,
					tangents,
					indices,
					material,
					meshParams));

				SetTransformValues(current, &parent->GetRenderMeshes().back()->GetTransform());
			}
		} // End RenderMesh population

		// Load other attachments now the GameObject transformations have been populated:
		if (current->light)
		{
			LoadAddLight(scene, parent, current->light);
		}

		if (current->camera)
		{
			LoadAddCamera(scene, parent, current->camera);
		}

		scene.AddGameObject(parent);
		
		if (current->children_count > 0)
		{
			for (size_t i = 0; i < current->children_count; i++)
			{
				const string childName = current->children[i]->name ? current->children[i]->name : "Unnamed node";
				shared_ptr<GameObject> childNode = make_shared<GameObject>(childName);
				childNode->GetTransform()->SetParent(parent->GetTransform()); // TODO: GameObject ctors should all take a parent Transform*

				LoadObjectHierarchyRecursiveHelper(rootPath, scene, data, current->children[i], childNode);
			}
		}
	}

	// Note: data must already be populated by calling cgltf_load_buffers
	void LoadSceneHierarchy(std::string const& rootPath, fr::SceneData& scene, cgltf_data* data)
	{
		LOG("Scene has %d object nodes", data->nodes_count);

		SEAssert("Loading > 1 scene is currently unsupported", data->scenes_count == 1);

		// Each node is the root in a transformation hierarchy:
		for (size_t node = 0; node < data->scenes->nodes_count; node++)
		{
			SEAssert("Error: Node is not a root", data->scenes->nodes[node]->parent == nullptr);

			shared_ptr<GameObject> currentNode = make_shared<GameObject>(
				data->scenes->nodes[node]->name ? string(data->scenes->nodes[node]->name) : 
				"Unnamed_node_" + to_string(node));

			LoadObjectHierarchyRecursiveHelper(rootPath, scene, data, data->scenes->nodes[node], currentNode);
		}
	}
} // namespace



/**********************************************************************************************************************
* SceneData members:
***********************************************************************************************************************/

namespace fr
{
	void SceneData::Load()
	{
		// TODO: Switch scene loading command line arguments to provide full relative paths/filenames
		// -> This will allow loading of arbitrary scenes, without folder/filename restriction
		const string rootpath = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + m_name + "\\";
		const string filepath =	rootpath + m_name + ".gltf";


		// Parse the GLTF file data:
		cgltf_options options = { (cgltf_file_type)0 };
		cgltf_data* data = NULL;
		cgltf_result parseResult = cgltf_parse_file(&options, filepath.c_str(), &data);
		SEAssert("Failed to parse scene file \"" + filepath + "\"", parseResult == cgltf_result_success);

		cgltf_result bufferLoadResult = cgltf_load_buffers(&options, data, filepath.c_str());
		SEAssert("Failed to load scene data \"" + filepath + "\"", bufferLoadResult == cgltf_result_success);

		// TODO: Add a cmd line flag to validated GLTF files, for efficiency?
		cgltf_result validationResult = cgltf_validate(data);
		SEAssert("GLTF file failed validation!", validationResult == cgltf_result_success);
		
		// Pre-reserve our vectors:
		m_gameObjects.reserve(max((int)data->nodes_count, 10));
		m_renderMeshes.reserve(max((int)data->meshes_count, 10));
		m_meshes.reserve(max((int)data->meshes_count, 10));
		m_textures.reserve(max((int)data->textures_count, 10));
		m_materials.reserve(max((int)data->materials_count, 10));
		m_pointLights.reserve(max((int)data->lights_count, 10)); // Probably an over-estimation
		m_cameras.reserve(max((int)data->cameras_count, 5));
		
		LoadSceneHierarchy(rootpath, *this, data);
		LoadAddCamera(*this, nullptr, nullptr); // Adds a default camera if none were found during LoadSceneHierarchy()
		
		AttachShadowMaps(*this); // TODO: Move this functionality into the Light ctor

		// Cleanup:
		cgltf_free(data);
	}


	SceneData::SceneData(string const& sceneName) :
		m_name(sceneName),
		m_ambientLight(nullptr),
		m_keyLight(nullptr)
	{
	}


	void SceneData::Destroy()
	{
		m_gameObjects.clear();
		m_renderMeshes.clear();
		m_meshes.clear();
		m_textures.clear();
		m_materials.clear();
		m_ambientLight = nullptr;
		m_keyLight = nullptr;
		m_pointLights.clear();
		m_cameras.clear();
		m_sceneWorldSpaceBounds = Bounds();
	}


	void SceneData::AddCamera(std::shared_ptr<gr::Camera> newCamera)
	{
		SEAssert("Cannot add a null camera", newCamera != nullptr);
		m_cameras.emplace_back(newCamera);
	}


	void SceneData::AddLight(std::shared_ptr<Light> newLight)
	{
		// TODO: Seems arbitrary that we cannot duplicate directional (and even ambient?) lights... Why even bother 
		// enforcing this? Just treat all lights the same

		switch (newLight->Type())
		{
		case Light::AmbientIBL:
		{
			SEAssert("Ambient light already exists, cannot have 2 ambient lights", m_ambientLight == nullptr);
			m_ambientLight = newLight;
		}
		break;
		case Light::Directional:
		{
			SEAssert("Direction light already exists, cannot have 2 directional lights", m_keyLight == nullptr);
			m_keyLight = newLight;
		}
		break;
		case Light::Point:
		{
			m_pointLights.emplace_back(newLight);
		}
		break;
		case Light::Spot:
		case Light::Area:
		case Light::Tube:
		default:
			LOG_ERROR("Ignorring unsupported light type");
		break;
		}
	}

	
	void SceneData::AddGameObject(std::shared_ptr<fr::GameObject> newGameObject)
	{
		m_gameObjects.emplace_back(newGameObject);

		for (size_t i = 0; i < newGameObject->GetRenderMeshes().size(); i++)
		{
			AddRenderMesh(newGameObject->GetRenderMeshes()[i]);
		}
	}


	void SceneData::AddRenderMesh(std::shared_ptr<gr::RenderMesh> newRenderMesh)
	{
		m_renderMeshes.emplace_back(); // Add the rendermesh to our tracking list
		
		for (shared_ptr<Mesh> mesh : newRenderMesh->GetChildMeshPrimitives())
		{
			// Add the mesh to our tracking array:
			m_meshes.push_back(mesh);

			UpdateSceneBounds(mesh);
			// TODO: Bounds management should belong to a RenderMesh object (not the mesh primitives)
		}
	}


	void SceneData::UpdateSceneBounds(std::shared_ptr<gr::Mesh> mesh)
	{
		// Update scene (world) bounds to contain the new mesh:
		Bounds meshWorldBounds(mesh->GetLocalBounds().GetTransformedBounds(mesh->GetTransform().GetWorldMatrix()));

		if (meshWorldBounds.xMin() < m_sceneWorldSpaceBounds.xMin())
		{
			m_sceneWorldSpaceBounds.xMin() = meshWorldBounds.xMin();
		}
		if (meshWorldBounds.xMax() > m_sceneWorldSpaceBounds.xMax())
		{
			m_sceneWorldSpaceBounds.xMax() = meshWorldBounds.xMax();
		}

		if (meshWorldBounds.yMin() < m_sceneWorldSpaceBounds.yMin())
		{
			m_sceneWorldSpaceBounds.yMin() = meshWorldBounds.yMin();
		}
		if (meshWorldBounds.yMax() > m_sceneWorldSpaceBounds.yMax())
		{
			m_sceneWorldSpaceBounds.yMax() = meshWorldBounds.yMax();
		}

		if (meshWorldBounds.zMin() < m_sceneWorldSpaceBounds.zMin())
		{
			m_sceneWorldSpaceBounds.zMin() = meshWorldBounds.zMin();
		}
		if (meshWorldBounds.zMax() > m_sceneWorldSpaceBounds.zMax())
		{
			m_sceneWorldSpaceBounds.zMax() = meshWorldBounds.zMax();
		}
	}


	void SceneData::AddUniqueTexture(shared_ptr<gr::Texture>& newTexture)
	{
		SEAssert("Cannot add null texture to textures table", newTexture != nullptr);

		unordered_map<string, shared_ptr<gr::Texture>>::const_iterator texturePosition =
			m_textures.find(newTexture->GetTexturePath());
		if (texturePosition != m_textures.end()) // Found existing
		{
			newTexture = texturePosition->second;
		}
		else  // Add new
		{
			m_textures[newTexture->GetTexturePath()] = newTexture;
			LOG("Texture \"%s\" registered with scene", newTexture->GetTexturePath().c_str());
		}
	}


	shared_ptr<Texture> SceneData::GetLoadTextureByPath(vector<string> texturePaths, bool returnErrorTex /*= false*/)
	{
		SEAssert("Expected either 1 or 6 texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);

		unordered_map<string, shared_ptr<gr::Texture>>::const_iterator texturePosition =
			m_textures.find(texturePaths[0]);
		if (texturePosition != m_textures.end())
		{
			LOG("Texture(s) at \"%s\" has already been loaded", texturePaths[0].c_str());
			return texturePosition->second;
		}

		shared_ptr<gr::Texture> result = LoadTextureFileFromPath(vector<string>(texturePaths), returnErrorTex);
		if (result)
		{
			AddUniqueTexture(result);
		}
		return result;
	}


	void SceneData::AddUniqueMaterial(shared_ptr<Material>& newMaterial)
	{
		SEAssert("Cannot add null material to material table", newMaterial != nullptr);

		// Note: Materials are uniquely identified by name, regardless of the MaterialDefinition they might use
		unordered_map<string, shared_ptr<gr::Material>>::const_iterator matPosition = 
			m_materials.find(newMaterial->Name());		
		if (matPosition != m_materials.end()) // Found existing
		{
			newMaterial = matPosition->second;
		}
		else // Add new
		{
			m_materials[newMaterial->Name()] = newMaterial;
			LOG("Material \"%s\" registered with scene", newMaterial->Name().c_str());
		}
	}


	std::shared_ptr<gr::Material> const SceneData::GetMaterial(std::string const& materialName) const
	{
		unordered_map<string, shared_ptr<gr::Material>>::const_iterator matPos = m_materials.find(materialName);
		SEAssert("Could not find material", matPos != m_materials.end());

		return matPos->second;
	}
}