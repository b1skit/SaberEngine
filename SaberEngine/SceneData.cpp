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
#include "VertexAttributeBuilder.h"
#include "Light.h"
#include "Camera.h"
#include "SceneObject.h"
#include "Mesh.h"
#include "MeshPrimitive.h"
#include "Transform.h"
#include "Material.h"
#include "Light.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "ShadowMap.h"
#include "ParameterBlock.h"

using gr::Camera;
using gr::Light;
using gr::Texture;
using gr::Material;
using re::MeshPrimitive;
using re::Bounds;
using gr::Transform;
using gr::Light;
using gr::ShadowMap;
using re::ParameterBlock;
using fr::SceneObject;
using en::Config;
using en::NamedObject;
using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::unordered_map;
using std::stringstream;
using std::max;
using std::to_string;
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


	void CopyImageData(
		std::vector<uint8_t>& texels,
		const uint8_t* imageData,
		size_t width,
		size_t height,
		uint8_t numChannels,
		uint8_t bitDepth,
		size_t firstTexelIndex) // firstTexelIndex is in units of # of pixels (NOT bytes)
	{	
		SEAssert("Invalid bit depth", bitDepth == 8 || bitDepth == 16 || bitDepth == 32);
		SEAssert("Invalid number of channels", numChannels >= 1 && numChannels <= 4);

		const uint8_t bytesPerPixel = (bitDepth * numChannels) / 8;
		const size_t numBytes = width * height * bytesPerPixel;

		SEAssert("Texels is not correctly allocated", numBytes == texels.size());

		const size_t firstTexelIdx = firstTexelIndex * bytesPerPixel;

		memcpy(&texels.at(firstTexelIdx), imageData, numBytes);
	}


	std::shared_ptr<gr::Texture> LoadTextureFileFromPath(vector<string> texturePaths, bool returnErrorTex = false)
	{
		SEAssert("Can load single faces or cubemaps only", texturePaths.size() == 1 || texturePaths.size() == 6);
		SEAssert("Invalid number of texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);

		LOG("Attempting to load %d textures: \"%s\"...", texturePaths.size(), texturePaths[0].c_str());

		// Flip the y-axis on loading (so pixel (0,0) is in the bottom-left of the image if using OpenGL
		platform::RenderingAPI const& api = Config::Get()->GetRenderingAPI();
		const bool flipY = api == platform::RenderingAPI::OpenGL ? true : false;

		stbi_set_flip_vertically_on_load(flipY);

		const uint32_t totalFaces = (uint32_t)texturePaths.size();

		// Start with parameters suitable for a generic error texture:
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
		texParams.m_useMIPs = true;

		// Load the texture, face-by-face:
		shared_ptr<Texture> texture(nullptr);
		for (size_t face = 0; face < totalFaces; face++)
		{
			// Get the image data:
			int width, height;
			int numChannels;
			uint8_t bitDepth = 0;
			void* imageData = nullptr;			

			if (stbi_is_hdr(texturePaths[face].c_str())) // HDR
			{
				imageData = stbi_loadf(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 32;
			}
			else if (stbi_is_16_bit(texturePaths[face].c_str()))
			{
				imageData = stbi_load_16(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 16;
			}
			else // Non-HDR
			{
				imageData = stbi_load(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 8;
			}

			if (imageData)
			{
				LOG("Found %dx%d, %d-bit texture with %d channels", width, height, bitDepth, numChannels);

				if (texture == nullptr) // 1st face
				{
					// Update the texture parameters:
					texParams.m_width = width;
					texParams.m_height = height;

					if ((width == 1 || height == 1) && (width != height))
					{
						LOG_WARNING("Found 1D texture, but 1D textures are currently not supported. Treating "
							"this texture as 2D");
						texParams.m_texDimension = gr::Texture::TextureDimension::Texture2D; // TODO: Support 1D textures
						/*texParams.m_texDimension = gr::Texture::TextureDimension::Texture1D;*/
					}

					switch (numChannels)
					{
					case 1:
					{
						if (bitDepth == 8) texParams.m_texFormat = gr::Texture::TextureFormat::R8;
						else if (bitDepth == 16) texParams.m_texFormat = gr::Texture::TextureFormat::R16F;
						else texParams.m_texFormat = gr::Texture::TextureFormat::R32F;
					}
					break;
					case 2:
					{
						if (bitDepth == 8) texParams.m_texFormat = gr::Texture::TextureFormat::RG8;
						else if (bitDepth == 16) texParams.m_texFormat = gr::Texture::TextureFormat::RG16F;
						else texParams.m_texFormat = gr::Texture::TextureFormat::RG32F;
					}
					break;
					case 3:
					{
						if (bitDepth == 8) texParams.m_texFormat = gr::Texture::TextureFormat::RGB8;
						else if (bitDepth == 16) texParams.m_texFormat = gr::Texture::TextureFormat::RGB16F;
						else texParams.m_texFormat = gr::Texture::TextureFormat::RGB32F;
					}
					break;
					case 4:
					{
						if (bitDepth == 8) texParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
						else if (bitDepth == 16) texParams.m_texFormat = gr::Texture::TextureFormat::RGBA16F;
						else texParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F;
					}
					break;
					default:
						SEAssertF("Invalid number of channels");
					}
					
					texParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color

					// Create the texture now the params are configured:
					texture = std::make_shared<gr::Texture>(texturePaths[0], texParams);
				}
				else // texture already exists: Ensure the face has the same dimensions
				{
					SEAssert("Parameter mismatch", texParams.m_width == width && texParams.m_height == height);
				}

				// Copy the data to our texture's texel vector:
				const size_t firstTexelIndex = face * width * height;
				CopyImageData(
					texture->Texels(), 
					static_cast<uint8_t const*>(imageData), 
					width, 
					height, 
					(int8_t)numChannels,
					bitDepth, 
					firstTexelIndex);

				// Cleanup:
				stbi_image_free(imageData);
			}
			else if (returnErrorTex)
			{
				if (texture != nullptr)
				{
					texture = nullptr;

					// Reset texParams to be suitable for an error texture
					texParams.m_width = 2;
					texParams.m_height = 2;
					texParams.m_texDimension = totalFaces == 1 ?
						gr::Texture::TextureDimension::Texture2D : gr::Texture::TextureDimension::TextureCubeMap;
					texParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
					texParams.m_texColorSpace = Texture::TextureColorSpace::Unknown;

					texParams.m_clearColor = ERROR_TEXTURE_COLOR_VEC4;
					texParams.m_useMIPs = true;
				}
				texture = std::make_shared<gr::Texture>(ERROR_TEXTURE_NAME, texParams);
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
		fr::SceneData& scene, std::string const& sceneRootPath, cgltf_material const* material)
	{
		const string matName = material == nullptr ? "MissingMaterial" : GenerateMaterialName(*material);
		if (scene.MaterialExists(matName))
		{
			return scene.GetMaterial(matName);
		}

		if (material == nullptr)
		{
			LOG_ERROR("Mesh does not have a material. Creating an error material");
			shared_ptr<Material> newMat =
				make_shared<Material>(matName, Material::GetMaterialDefinition("pbrMetallicRoughness"));

			for (size_t i = 0; i < 5; i++)
			{
				// TODO: Currently, this inserts the error color as the normal texture, which is invalid
				newMat->GetTexture((uint32_t)i) = scene.GetLoadTextureByPath({ ERROR_TEXTURE_NAME }, true);
				newMat->GetTexture((uint32_t)i)->Create();
			}

			newMat->GetParameterBlock() = ParameterBlock::Create(
				"PBRMetallicRoughnessParams", 
				Material::PBRMetallicRoughnessParams(), 
				re::ParameterBlock::UpdateType::Immutable,
				re::ParameterBlock::Lifetime::Permanent);

			scene.AddUniqueMaterial(newMat);
			return newMat;
		}

		SEAssert("Unsupported material model", material->has_pbr_metallic_roughness == 1);	

		shared_ptr<Material> newMat =
			make_shared<Material>(matName, Material::GetMaterialDefinition("pbrMetallicRoughness"));

		newMat->GetShader() = nullptr; // Not required; just for clarity
			
		auto LoadTextureOrColor = [&](
			cgltf_texture* texture, 
			vec4 const& colorFallback, 
			Texture::TextureFormat formatFallback,
			Texture::TextureColorSpace colorSpace)
		{
			SEAssert("Invalid fallback format", 
				formatFallback != Texture::TextureFormat::Depth32F && formatFallback != Texture::TextureFormat::Invalid);

			shared_ptr<Texture> tex;
			if (texture && texture->image && texture->image->uri)
			{
				tex = scene.GetLoadTextureByPath(
					{ sceneRootPath + texture->image->uri });

				Texture::TextureParams texParams = tex->GetTextureParams();
				texParams.m_texColorSpace = colorSpace;
				tex->SetTextureParams(texParams);
			}
			else
			{
				Texture::TextureParams colorTexParams;
				colorTexParams.m_clearColor = colorFallback; // Clear color = initial fill color
				colorTexParams.m_texFormat = formatFallback;
				colorTexParams.m_texColorSpace = colorSpace;				

				// Construct a name:
				const size_t numChannels = Texture::GetNumberOfChannels(formatFallback);
				string texName = "Color_" + to_string(colorTexParams.m_clearColor.x) + "_";
				if (numChannels >= 2)
				{
					texName += to_string(colorTexParams.m_clearColor.y) + "_";
					if (numChannels >= 3)
					{
						texName += to_string(colorTexParams.m_clearColor.z) + "_";
						if (numChannels >= 4)
						{
							texName += to_string(colorTexParams.m_clearColor.w) + "_";
						}
					}
				}				
				texName += (colorSpace == Texture::TextureColorSpace::sRGB ? "sRGB" : "Linear");

				tex = make_shared<Texture>(texName, colorTexParams);
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
			Texture::TextureFormat::RGB8,
			Texture::TextureColorSpace::sRGB);

		// MatMetallicRoughness
		newMat->GetTexture(1) = LoadTextureOrColor(
			material->pbr_metallic_roughness.metallic_roughness_texture.texture,
			missingTextureColor,
			Texture::TextureFormat::RGB8,
			Texture::TextureColorSpace::Linear);

		// MatNormal
		newMat->GetTexture(2) = LoadTextureOrColor(
			material->normal_texture.texture,
			vec4(0.5f, 0.5f, 1.0f, 0.0f), // Equivalent to a [0,0,1] normal after unpacking
			Texture::TextureFormat::RGB8,
			Texture::TextureColorSpace::Linear);

		// MatOcclusion
		newMat->GetTexture(3) = LoadTextureOrColor(
			material->occlusion_texture.texture,
			missingTextureColor,	// Completely unoccluded
			Texture::TextureFormat::RGB8,
			Texture::TextureColorSpace::Linear);

		// MatEmissive
		newMat->GetTexture(4) = LoadTextureOrColor(
			material->emissive_texture.texture,
			missingTextureColor,
			Texture::TextureFormat::RGB8,
			Texture::TextureColorSpace::sRGB); // GLTF convention: Must be converted to linear before use

		// Construct a permanent parameter block for the material params:
		Material::PBRMetallicRoughnessParams matParams;
		matParams.g_baseColorFactor = glm::make_vec4(material->pbr_metallic_roughness.base_color_factor);
		matParams.g_metallicFactor = material->pbr_metallic_roughness.metallic_factor;
		matParams.g_roughnessFactor = material->pbr_metallic_roughness.roughness_factor;
		matParams.g_normalScale = material->normal_texture.texture ? material->normal_texture.scale : 1.0f;
		matParams.g_occlusionStrength = material->occlusion_texture.texture ? material->occlusion_texture.scale : 1.0f;
		matParams.g_emissiveStrength = material->has_emissive_strength ? material->emissive_strength.emissive_strength : 1.0f;
		matParams.g_emissiveFactor = glm::make_vec3(material->emissive_factor);
		matParams.g_f0 = vec3(0.04f, 0.04f, 0.04f);

		// TODO: Material MatParams should be passed as a ctor argument
		newMat->GetParameterBlock() = ParameterBlock::Create(
			"PBRMetallicRoughnessParams", 
			matParams, 
			ParameterBlock::UpdateType::Immutable, 
			ParameterBlock::Lifetime::Permanent);

		scene.AddUniqueMaterial(newMat);
		return newMat;
	}


	// Creates a default camera if camera == nullptr, and no cameras exist in scene
	void LoadAddCamera(fr::SceneData& scene, shared_ptr<SceneObject> parent, cgltf_camera* camera)
	{
		if (camera == nullptr && parent == nullptr)
		{
			if (scene.GetCameras().size() == 0) // Create a default camera at the origin
			{
				LOG("\nCreating a default camera");

				gr::Camera::CameraConfig camConfig;
				camConfig.m_aspectRatio = Config::Get()->GetWindowAspectRatio();
				camConfig.m_fieldOfView = Config::Get()->GetValue<float>("defaultFieldOfView");
				camConfig.m_near = Config::Get()->GetValue<float>("defaultNear");
				camConfig.m_far = Config::Get()->GetValue<float>("defaultFar");
				camConfig.m_exposure = Config::Get()->GetValue<float>("defaultExposure");

				scene.AddCamera(make_shared<Camera>("Default camera", camConfig, nullptr));
			}

			return;
		}

		SEAssert("Must supply a parent and camera pointer", parent != nullptr && camera != nullptr);

		const string camName = camera->name ? string(camera->name) : "Unnamed camera";
		LOG("Loading camera \"%s\"", camName.c_str());

		gr::Camera::CameraConfig camConfig;
		camConfig.m_projectionType = camera->type == cgltf_camera_type_orthographic ? 
			Camera::CameraConfig::ProjectionType::Orthographic : Camera::CameraConfig::ProjectionType::Perspective;
		if (camConfig.m_projectionType == Camera::CameraConfig::ProjectionType::Orthographic)
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

		shared_ptr<Camera> newCam = make_shared<Camera>(camName, camConfig, parent->GetTransform());
		scene.AddCamera(newCam);
	}


	void LoadAddLight(fr::SceneData& scene, shared_ptr<SceneObject> parent, cgltf_light* light)
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
			SEAssertF("Invalid light type");
		}

		const vec3 colorIntensity = glm::make_vec3(light->color) * light->intensity;
		const bool attachShadow = true;
		shared_ptr<Light> newLight = 
			make_shared<Light>(lightName, parent->GetTransform(), lightType, colorIntensity, attachShadow);

		scene.AddLight(newLight);
	}


	// Depth-first traversal
	void LoadObjectHierarchyRecursiveHelper(
		std::string const& sceneRootPath, fr::SceneData& scene, cgltf_data* data, cgltf_node* current, shared_ptr<SceneObject> parent)
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

		// Set the SceneObject transform:
		if (current->mesh == nullptr)
		{
			SetTransformValues(current, parent->GetTransform());
		}
		else // Node has a mesh: Create a mesh primitive and attach it to a Mesh
		{
			// Add each MeshPrimitive as a child of the SceneObject's Mesh:
			for (size_t primitive = 0; primitive < current->mesh->primitives_count; primitive++)
			{
				SEAssert(
					"TODO: Support more primitive types/draw modes!",
					current->mesh->primitives[primitive].type == cgltf_primitive_type::cgltf_primitive_type_triangles);

				// Populate the mesh params:
				MeshPrimitive::MeshPrimitiveParams meshPrimitiveParams;
				switch (current->mesh->primitives[primitive].type)
				{
					case cgltf_primitive_type::cgltf_primitive_type_points:
					{
						meshPrimitiveParams.m_drawMode = MeshPrimitive::DrawMode::Points; 
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_lines:
					{
						meshPrimitiveParams.m_drawMode = MeshPrimitive::DrawMode::Lines;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_line_loop:
					{
						meshPrimitiveParams.m_drawMode = MeshPrimitive::DrawMode::LineLoop;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_line_strip:
					{
						meshPrimitiveParams.m_drawMode = MeshPrimitive::DrawMode::LineStrip;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_triangles:
					{
						meshPrimitiveParams.m_drawMode = MeshPrimitive::DrawMode::Triangles;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_triangle_strip:
					{
						meshPrimitiveParams.m_drawMode = MeshPrimitive::DrawMode::TriangleStrip;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_triangle_fan:
					{
						meshPrimitiveParams.m_drawMode = MeshPrimitive::DrawMode::TriangleFan;
					}
					break;
					case cgltf_primitive_type::cgltf_primitive_type_max_enum:
					default:
						SEAssertF("Unsupported primitive type/draw mode");
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
					// -> Override the MeshPrimitive ctor!

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
						SEAssertF("Invalid attribute type");
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
							Config::Get()->GetRenderingAPI();
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
					&meshPrimitiveParams,
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
					LoadAddMaterial(scene, sceneRootPath, current->mesh->primitives[primitive].material);

				// Attach the primitive:
				parent->AddMeshPrimitive(make_shared<MeshPrimitive>(
					nodeName,
					positions,
					normals,
					colors,
					uv0,
					tangents,
					indices,
					material,
					meshPrimitiveParams,
					nullptr));

				SetTransformValues(current, &parent->GetMeshes().back()->GetTransform());
			}
		} // End Mesh population

		// Add other attachments now the SceneObject transformations have been populated:
		if (current->light)
		{
			LoadAddLight(scene, parent, current->light);
		}

		if (current->camera)
		{
			LoadAddCamera(scene, parent, current->camera);
		}

		scene.AddSceneObject(parent);
		
		if (current->children_count > 0)
		{
			for (size_t i = 0; i < current->children_count; i++)
			{
				const string childName = current->children[i]->name ? current->children[i]->name : "Unnamed node";
				shared_ptr<SceneObject> childNode = make_shared<SceneObject>(childName, parent->GetTransform());

				LoadObjectHierarchyRecursiveHelper(sceneRootPath, scene, data, current->children[i], childNode);
			}
		}
	}

	// Note: data must already be populated by calling cgltf_load_buffers
	void LoadSceneHierarchy(std::string const& sceneRootPath, fr::SceneData& scene, cgltf_data* data)
	{
		LOG("Scene has %d object nodes", data->nodes_count);

		SEAssert("Loading > 1 scene is currently unsupported", data->scenes_count == 1);

		// Each node is the root in a transformation hierarchy:
		for (size_t node = 0; node < data->scenes->nodes_count; node++)
		{
			SEAssert("Error: Node is not a root", data->scenes->nodes[node]->parent == nullptr);

			shared_ptr<SceneObject> currentNode = make_shared<SceneObject>(
				data->scenes->nodes[node]->name ? string(data->scenes->nodes[node]->name) : 
				"Unnamed_node_" + to_string(node),
				nullptr); // Root node has no parent

			LoadObjectHierarchyRecursiveHelper(sceneRootPath, scene, data, data->scenes->nodes[node], currentNode);
		}
	}
} // namespace



/**********************************************************************************************************************
* SceneData members:
***********************************************************************************************************************/

namespace fr
{
	bool SceneData::Load(string const& sceneFilePath)
	{
		if (sceneFilePath.empty())
		{
			SEAssert("No scene name received. Did you forget to use the \"-scene theSceneName\" command line "
				"argument?", !sceneFilePath.empty());

			return false;
		}

		// Parse the GLTF file data:
		cgltf_options options = { (cgltf_file_type)0 };
		cgltf_data* data = NULL;
		cgltf_result parseResult = cgltf_parse_file(&options, sceneFilePath.c_str(), &data);
		if (parseResult != cgltf_result::cgltf_result_success)
		{
			SEAssert("Failed to parse scene file \"" + sceneFilePath + "\"", parseResult == cgltf_result_success);
			return false;
		}
		
		cgltf_result bufferLoadResult = cgltf_load_buffers(&options, data, sceneFilePath.c_str());
		if (bufferLoadResult != cgltf_result::cgltf_result_success)
		{
			SEAssert("Failed to load scene data \"" + sceneFilePath + "\"", bufferLoadResult == cgltf_result_success);
			return false;
		}		

		// TODO: Add a cmd line flag to validated GLTF files, for efficiency?
		cgltf_result validationResult = cgltf_validate(data);
		if (validationResult != cgltf_result::cgltf_result_success)
		{
			SEAssert("GLTF file failed validation!", validationResult == cgltf_result_success);
			return false;
		}
		
		// Pre-reserve our vectors:
		m_updateables.reserve(max((int)data->nodes_count, 10));
		m_meshes.reserve(max((int)data->meshes_count, 10));
		m_meshPrimitives.reserve(max((int)data->meshes_count, 10));
		m_textures.reserve(max((int)data->textures_count, 10));
		m_materials.reserve(max((int)data->materials_count, 10));
		m_pointLights.reserve(max((int)data->lights_count, 10)); // Probably an over-estimation
		m_cameras.reserve(max((int)data->cameras_count, 5));
		
		const string sceneRootPath = Config::Get()->GetValue<string>("sceneRootPath");
		LoadSceneHierarchy(sceneRootPath, *this, data);
		LoadAddCamera(*this, nullptr, nullptr); // Adds a default camera if none were found during LoadSceneHierarchy()

		// Cleanup:
		cgltf_free(data);

		return true;
	}


	SceneData::SceneData(string const& sceneName) :
			NamedObject(sceneName),
		m_ambientLight(nullptr),
		m_keyLight(nullptr)
	{
	}


	void SceneData::Destroy()
	{
		m_updateables.clear();
		m_meshes.clear();
		m_meshPrimitives.clear();
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
		m_updateables.emplace_back(newCamera);
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

		// Updateables get pumped every frame:
		m_updateables.emplace_back(newLight);
	}

	
	void SceneData::AddSceneObject(std::shared_ptr<fr::SceneObject> sceneObject)
	{
		m_updateables.emplace_back(sceneObject);

		for (size_t i = 0; i < sceneObject->GetMeshes().size(); i++)
		{
			AddMesh(sceneObject->GetMeshes()[i]);
		}
	}


	void SceneData::AddMesh(std::shared_ptr<gr::Mesh> mesh)
	{
		m_meshes.emplace_back(mesh); // Add the mesh to our tracking list
		
		for (shared_ptr<MeshPrimitive> meshPrimitive : mesh->GetMeshPrimitives())
		{
			// Add the mesh to our tracking array:
			m_meshPrimitives.push_back(meshPrimitive);

			UpdateSceneBounds(meshPrimitive);
			// TODO: Bounds management should belong to a Mesh object (not the mesh primitives)
		}
	}


	void SceneData::AddUpdateable(std::shared_ptr<en::Updateable> updateable)
	{
		m_updateables.emplace_back(updateable);
	}


	void SceneData::UpdateSceneBounds(std::shared_ptr<re::MeshPrimitive> meshPrimitive)
	{
		// Update scene (world) bounds to contain the new mesh primitive:
		Bounds meshWorldBounds(
			meshPrimitive->GetLocalBounds().GetTransformedBounds(meshPrimitive->GetOwnerTransform()->GetWorldMatrix()));

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

		unordered_map<size_t, shared_ptr<gr::Texture>>::const_iterator texturePosition = 
			m_textures.find(newTexture->GetNameID());
		if (texturePosition != m_textures.end()) // Found existing
		{
			newTexture = texturePosition->second;
		}
		else  // Add new
		{
			m_textures[newTexture->GetNameID()] = newTexture;
			LOG("Texture \"%s\" registered with scene", newTexture->GetName().c_str());
		}
	}


	shared_ptr<Texture> SceneData::GetLoadTextureByPath(vector<string> texturePaths, bool returnErrorTex /*= false*/)
	{
		SEAssert("Expected either 1 or 6 texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);

		const size_t nameID = NamedObject::ComputeIDFromName(texturePaths[0]);

		unordered_map<size_t, shared_ptr<gr::Texture>>::const_iterator texturePosition = m_textures.find(nameID);
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
		unordered_map<size_t, shared_ptr<gr::Material>>::const_iterator matPosition = 
			m_materials.find(newMaterial->GetNameID());
		if (matPosition != m_materials.end()) // Found existing
		{
			newMaterial = matPosition->second;
		}
		else // Add new
		{
			m_materials[newMaterial->GetNameID()] = newMaterial;
			LOG("Material \"%s\" registered with scene", newMaterial->GetName().c_str());
		}
	}


	std::shared_ptr<gr::Material> const SceneData::GetMaterial(std::string const& materialName) const
	{
		const size_t nameID = NamedObject::ComputeIDFromName(materialName);
		unordered_map<size_t, shared_ptr<gr::Material>>::const_iterator matPos = m_materials.find(nameID);
		SEAssert("Could not find material", matPos != m_materials.end());

		return matPos->second;
	}


	bool SceneData::MaterialExists(std::string const& matName) const
	{ 
		const size_t nameID = NamedObject::ComputeIDFromName(matName);
		return m_materials.find(nameID) != m_materials.end();
	}
}