// © 2022 Adam Badke. All rights reserved.

// Note: We can't include STBI in our pch, as the following define can only be included ONCE in the project
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#pragma warning(disable : 4996) // Suppress error C4996 (Caused by use of fopen, strcpy, strncpy in cgltf.h)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "Camera.h"
#include "Config.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Light.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshPrimitive.h"
#include "ParameterBlock.h"
#include "PerformanceTimer.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneNode.h"
#include "Shader.h"
#include "ShadowMap.h"
#include "ThreadPool.h"
#include "ThreadSafeVector.h"
#include "Transform.h"
#include "VertexStreamBuilder.h"

using fr::SceneData;
using gr::Camera;
using gr::Light;
using re::Texture;
using gr::Material;
using re::MeshPrimitive;
using gr::Bounds;
using gr::Transform;
using re::ParameterBlock;
using fr::SceneNode;
using en::Config;
using en::CoreEngine;
using util::PerformanceTimer;
using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::stringstream;
using std::to_string;
using glm::quat;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4;
using glm::make_mat4;
using std::unordered_map;
using std::max;


// Data loading helpers:
namespace
{
	constexpr glm::vec4 k_errorTextureColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
	constexpr char k_missingMaterialName[] = "MissingMaterial";


	// STBI Image data loading helpers:
	/*****************************************************************************************************************/


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
		SEAssert("Invalid number of channels", numChannels == 1 || numChannels == 2 || numChannels == 4);

		const uint8_t bytesPerPixel = (bitDepth * numChannels) / 8;
		const size_t numBytes = width * height * bytesPerPixel;

		SEAssert("Texels is not correctly allocated", numBytes == texels.size());

		const size_t firstByteIdx = firstTexelIndex * bytesPerPixel;

		memcpy(&texels.at(firstByteIdx), imageData, numBytes);
	}


	std::shared_ptr<re::Texture> LoadTextureFromFilePath(
		vector<string> texturePaths, 
		bool returnErrorTex, 
		glm::vec4 const& errorTexFillColor, 
		Texture::ColorSpace colorSpace)
	{
		SEAssert("Can load single faces or cubemaps only", texturePaths.size() == 1 || texturePaths.size() == 6);
		SEAssert("Invalid number of texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);

		LOG("Attempting to load %d texture(s): \"%s\"...", texturePaths.size(), texturePaths[0].c_str());

		PerformanceTimer timer;
		timer.Start();

		// TODO: We shouldn't set/reset this on every call
		stbi_set_flip_vertically_on_load(false);

		const uint32_t totalFaces = (uint32_t)texturePaths.size();

		// Modify default TextureParams to be suitable for a generic error texture:
		Texture::TextureParams texParams
		{
			.m_faces = totalFaces,

			.m_usage = re::Texture::Usage::Color,
			.m_dimension = (totalFaces == 1 ?
				re::Texture::Dimension::Texture2D : re::Texture::Dimension::TextureCubeMap),
			.m_format = re::Texture::Format::RGBA8,
			.m_colorSpace = colorSpace,
			.m_clearColor = errorTexFillColor,
		};
		

		// Load the texture, face-by-face:
		shared_ptr<Texture> texture(nullptr);
		for (size_t face = 0; face < totalFaces; face++)
		{
			// Get the image data:
			int width = 0;
			int height = 0;
			int numChannels = 0;			
			stbi_info(texturePaths[face].c_str(), &width, &height, &numChannels);

			// We don't support 3-channel textures, allow 1 or 2 channels, or force 4-channel instead
			const int desiredChannels = numChannels == 3 ? 4 : numChannels;

			uint8_t bitDepth = 0;
			void* imageData = nullptr;

			if (stbi_is_hdr(texturePaths[face].c_str())) // HDR
			{
				imageData = stbi_loadf(texturePaths[face].c_str(), &width, &height, &numChannels, desiredChannels);
				bitDepth = 32;
			}
			else if (stbi_is_16_bit(texturePaths[face].c_str()))
			{
				imageData = stbi_load_16(texturePaths[face].c_str(), &width, &height, &numChannels, desiredChannels);
				bitDepth = 16;
			}
			else // Non-HDR
			{
				imageData = stbi_load(texturePaths[face].c_str(), &width, &height, &numChannels, desiredChannels);
				bitDepth = 8;
			}

			if (imageData)
			{
				LOG("Texture \"%s\" is %dx%d, %d-bit, %d channels",
					texturePaths[face].c_str(), width, height, bitDepth, desiredChannels);

				if (texture == nullptr) // i.e. We're processing the 1st face
				{
					// Update the texture parameters:
					texParams.m_width = width;
					texParams.m_height = height;

					if ((width == 1 || height == 1) && (width != height))
					{
						LOG_WARNING("Found 1D texture, but 1D textures are currently not supported. Treating "
							"this texture as 2D");
						texParams.m_dimension = re::Texture::Dimension::Texture2D; // TODO: Support 1D textures
						/*texParams.m_dimension = re::Texture::Dimension::Texture1D;*/
					}

					switch (desiredChannels)
					{
					case 1:
					{
						if (bitDepth == 8) texParams.m_format = re::Texture::Format::R8;
						else if (bitDepth == 16) texParams.m_format = re::Texture::Format::R16F;
						else texParams.m_format = re::Texture::Format::R32F;
					}
					break;
					case 2:
					{
						if (bitDepth == 8) texParams.m_format = re::Texture::Format::RG8;
						else if (bitDepth == 16) texParams.m_format = re::Texture::Format::RG16F;
						else texParams.m_format = re::Texture::Format::RG32F;
					}
					break;
					case 4:
					{
						if (bitDepth == 8) texParams.m_format = re::Texture::Format::RGBA8;
						else if (bitDepth == 16) texParams.m_format = re::Texture::Format::RGBA16F;
						else texParams.m_format = re::Texture::Format::RGBA32F;
					}
					break;
					default:
						SEAssertF("Invalid number of channels");
					}
					
					texParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color

					// Create the texture now the params are configured:
					texture = re::Texture::Create(texturePaths[0], texParams, false);
				}
				else // texture already exists: Ensure the face has the same dimensions
				{
					SEAssert("Parameter mismatch", texParams.m_width == width && texParams.m_height == height);
				}

				// Copy the data to our texture's texel vector:
				const size_t firstTexelIndex = face * width * height;
				CopyImageData(
					texture->GetTexels(), 
					static_cast<uint8_t const*>(imageData), 
					width, 
					height, 
					(int8_t)desiredChannels,
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
					texParams.m_dimension = totalFaces == 1 ?
						re::Texture::Dimension::Texture2D : re::Texture::Dimension::TextureCubeMap;
					texParams.m_format = re::Texture::Format::RGBA8;
					texParams.m_colorSpace = Texture::ColorSpace::sRGB;

					texParams.m_clearColor = errorTexFillColor;
					texParams.m_useMIPs = true;
				}
				texture = re::Texture::Create(texturePaths[0], texParams, true);
			}
			else
			{
				char const* failResult = stbi_failure_reason();
				LOG_WARNING("Failed to load image \"%s\": %s", texturePaths[0].c_str(), failResult);
				timer.StopSec();
				return nullptr;
			}
		}

		LOG("Loaded texture \"%s\" in %f seconds...", texturePaths[0].c_str(), timer.StopSec());

		// Note: Texture color space must still be set
		return texture; 
	}


	std::shared_ptr<re::Texture> LoadTextureFromMemory(
		std::string const& texName,
		unsigned char const* texSrc,
		uint32_t texSrcNumBytes,
		Texture::ColorSpace colorSpace)
	{
		SEAssert("Invalid texture memory allocation", texSrc != nullptr && texSrcNumBytes > 0);
		
		LOG("Attempting to load texture \"%s\" from memory...", texName.c_str());
		PerformanceTimer timer;
		timer.Start();

		// TODO: We shouldn't set/reset this on every call
		stbi_set_flip_vertically_on_load(false);

		// Modify default TextureParams to be suitable for a generic error texture:
		Texture::TextureParams texParams
		{
			.m_usage = re::Texture::Usage::Color,
			.m_dimension = re::Texture::Dimension::Texture2D,
			.m_format = re::Texture::Format::RGBA8,
			.m_colorSpace = colorSpace,
			.m_clearColor = k_errorTextureColor
		};

		// Get the image data:
		int width = 0;
		int height = 0;
		int numChannels = 0;
		stbi_info_from_memory(static_cast<stbi_uc const*>(texSrc), texSrcNumBytes , &width, &height, &numChannels);

		// We don't support 3-channel textures, allow 1 or 2 channels, or force 4-channel instead
		const int desiredChannels = numChannels == 3 ? 4 : numChannels;

		uint8_t bitDepth = 0;
		void* imageData = nullptr;
		if (stbi_is_hdr_from_memory(texSrc, texSrcNumBytes))
		{
			imageData = stbi_loadf_from_memory(texSrc, texSrcNumBytes, &width, &height, &numChannels, desiredChannels);
			bitDepth = 32;
		}
		else if (stbi_is_16_bit_from_memory(texSrc, texSrcNumBytes))
		{
			imageData = stbi_load_16_from_memory(texSrc, texSrcNumBytes, &width, &height, &numChannels, desiredChannels);
			bitDepth = 16;
		}
		else // Non-HDR
		{
			imageData = stbi_load_from_memory(texSrc, texSrcNumBytes, &width, &height, &numChannels, desiredChannels);
			bitDepth = 8;
		}

		shared_ptr<Texture> texture(nullptr);
		if (imageData)
		{
			LOG("Texture \"%s\" is %dx%d, %d-bit, %d channels", 
				texName.c_str(), width, height, bitDepth, desiredChannels);

			// Update the texture parameters:
			texParams.m_width = width;
			texParams.m_height = height;

			if ((width == 1 || height == 1) && (width != height))
			{
				LOG_WARNING("Found 1D texture, but 1D textures are currently not supported. Treating "
					"this texture as 2D");
				texParams.m_dimension = re::Texture::Dimension::Texture2D; // TODO: Support 1D textures
				/*texParams.m_dimension = re::Texture::Dimension::Texture1D;*/
			}

			switch (desiredChannels)
			{
			case 1:
			{
				if (bitDepth == 8) texParams.m_format = re::Texture::Format::R8;
				else if (bitDepth == 16) texParams.m_format = re::Texture::Format::R16F;
				else texParams.m_format = re::Texture::Format::R32F;
			}
			break;
			case 2:
			{
				if (bitDepth == 8) texParams.m_format = re::Texture::Format::RG8;
				else if (bitDepth == 16) texParams.m_format = re::Texture::Format::RG16F;
				else texParams.m_format = re::Texture::Format::RG32F;
			}
			break;
			case 4:
			{
				if (bitDepth == 8) texParams.m_format = re::Texture::Format::RGBA8;
				else if (bitDepth == 16) texParams.m_format = re::Texture::Format::RGBA16F;
				else texParams.m_format = re::Texture::Format::RGBA32F;
			}
			break;
			default:
				SEAssertF("Invalid number of channels");
			}

			texParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color

			// Create the texture now the params are configured:
			texture = re::Texture::Create(texName, texParams, false);

			// Copy the data to our texture's texel vector:
			CopyImageData(
				texture->GetTexels(),
				static_cast<uint8_t const*>(imageData),
				width,
				height,
				(int8_t)desiredChannels,
				bitDepth,
				0); // 1st texel index
		}
		else
		{
			SEAssertF("Failed to load image data");
		}

		LOG("Loaded texture \"%s\" from memory in %f seconds...", texName.c_str(), timer.StopSec());

		// Note: Texture color space must still be set
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


	string GenerateTextureColorFallbackName(
		vec4 const& colorFallback, size_t numChannels, Texture::ColorSpace colorSpace)
	{
		string texName = "Color_" + to_string(colorFallback.x) + "_";
		if (numChannels >= 2)
		{
			texName += to_string(colorFallback.y) + "_";
			if (numChannels >= 3)
			{
				texName += to_string(colorFallback.z) + "_";
				if (numChannels >= 4)
				{
					texName += to_string(colorFallback.w) + "_";
				}
			}
		}
		texName += (colorSpace == Texture::ColorSpace::sRGB ? "sRGB" : "Linear");

		return texName;
	}


	// Assemble a name for textures loaded from memory: Either use the provided name, or create a unique one
	std::string GenerateEmbeddedTextureName(char const* texName)
	{
		string texNameStr;

		if (texName != nullptr)
		{
			texNameStr = string(texName);
		}
		else
		{
			static std::atomic<uint32_t> unnamedTexIdx = 0;
			const uint32_t thisTexIdx = unnamedTexIdx.fetch_add(1);
			texNameStr = "EmbeddedTexture_" + std::to_string(thisTexIdx);
		}

		return texNameStr;
	}


	shared_ptr<re::Texture> LoadTextureOrColor(
		SceneData& scene,
		string const& sceneRootPath, 
		cgltf_texture* texture, 
		vec4 const& colorFallback, 
		Texture::Format formatFallback, 
		Texture::ColorSpace colorSpace)
	{
		SEAssert("Invalid fallback format",
			formatFallback != Texture::Format::Depth32F && formatFallback != Texture::Format::Invalid);

		shared_ptr<Texture> tex;
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
					const std::string texNameStr = GenerateEmbeddedTextureName(texture->image->name);
					if (scene.TextureExists(texNameStr))
					{
						tex = scene.GetTexture(texNameStr);
					}
					else
					{
						tex = LoadTextureFromMemory(
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
					tex = LoadTextureFromFilePath({ texNameStr }, false, k_errorTextureColor, colorSpace);
				}
			}
			else if (texture->image->buffer_view) // texture data is already loaded in memory
			{
				const std::string texNameStr = GenerateEmbeddedTextureName(texture->image->name);

				if (scene.TextureExists(texNameStr))
				{
					tex = scene.GetTexture(texNameStr);
				}
				else
				{
					unsigned char const* texSrc = static_cast<unsigned char const*>(
						texture->image->buffer_view->buffer->data) + texture->image->buffer_view->offset;

					const uint32_t texSrcNumBytes = static_cast<uint32_t>(texture->image->buffer_view->size);
					tex = LoadTextureFromMemory(texNameStr, texSrc, texSrcNumBytes, colorSpace);
				}
			}

			SEAssert("Failed to load texture: Does the asset exist?", tex != nullptr);
		}
		else
		{
			// Create a texture color fallback:
			Texture::TextureParams colorTexParams
			{
				.m_usage = re::Texture::Usage::Color,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = formatFallback,
				.m_colorSpace = colorSpace,
				.m_clearColor = colorFallback
			};			

			const size_t numChannels = Texture::GetNumberOfChannels(formatFallback);
			const string fallbackName = GenerateTextureColorFallbackName(colorFallback, numChannels, colorSpace);

			if (scene.TextureExists(fallbackName))
			{
				tex = scene.GetTexture(fallbackName);
			}
			else
			{
				tex = re::Texture::Create(fallbackName, colorTexParams, true);
			}			
		}

		return tex;
	}


	void GenerateErrorMaterial(SceneData& scene)
	{
		LOG("Generating an error material \"%s\"...", k_missingMaterialName);

		constexpr char missingAlbedoTexName[]				= "MissingAlbedoTexture";
		constexpr char missingMetallicRoughnessTexName[]	= "MissingMetallicRoughnessTexture";
		constexpr char missingNormalTexName[]				= "MissingNormalTexture";
		constexpr char missingOcclusionTexName[]			= "MissingOcclusionTexture";
		constexpr char missingEmissiveTexName[]				= "MissingEmissiveTexture";

		std::shared_ptr<gr::Material> errorMat = 
			std::make_shared<gr::Material>(k_missingMaterialName, Material::GetMaterialDefinition("pbrMetallicRoughness"));

		// MatAlbedo
		std::shared_ptr<re::Texture> errorAlbedo = 
			LoadTextureFromFilePath({ missingAlbedoTexName }, true, k_errorTextureColor, re::Texture::ColorSpace::sRGB);
		errorMat->SetTexture(0, errorAlbedo);

		// MatMetallicRoughness
		std::shared_ptr<re::Texture> errorMetallicRoughness = LoadTextureFromFilePath(
			{ missingMetallicRoughnessTexName }, true, glm::vec4(0.f, 1.f, 0.f, 0.f), re::Texture::ColorSpace::Linear);
		errorMat->SetTexture(1, errorMetallicRoughness);

		// MatNormal
		std::shared_ptr<re::Texture> errorNormal = LoadTextureFromFilePath(
			{ missingNormalTexName }, true, glm::vec4(0.5f, 0.5f, 1.0f, 0.0f), re::Texture::ColorSpace::Linear);
		errorMat->SetTexture(2, errorNormal);

		// MatOcclusion
		std::shared_ptr<re::Texture> errorOcclusion = LoadTextureFromFilePath(
			{ missingOcclusionTexName }, true, glm::vec4(1.f), re::Texture::ColorSpace::Linear);
		errorMat->SetTexture(3, errorOcclusion);

		// MatEmissive
		std::shared_ptr<re::Texture> errorEmissive = LoadTextureFromFilePath(
			{ missingEmissiveTexName }, true, k_errorTextureColor, re::Texture::ColorSpace::sRGB);
		errorMat->SetTexture(4, errorEmissive);

		// Construct a default permanent parameter block for the material params:
		Material::PBRMetallicRoughnessParams matParams;
		matParams.g_f0 = vec3(0.04f, 0.04f, 0.04f);

		errorMat->SetParameterBlock(matParams);

		scene.AddUniqueMaterial(errorMat);
	}


	void PreLoadMaterials(std::string const& sceneRootPath, SceneData& scene, cgltf_data* data)
	{
		const size_t numMaterials = data->materials_count;
		LOG("Loading %d scene materials", numMaterials);

		// We assign each material to a thread; These threads will spawn new threads to load each texture. We need to 
		// wait on the future of each material to know when we can begin waiting on the futures for its textures
		std::vector<std::future<util::ThreadSafeVector<std::future<void>>>> matFutures;
		matFutures.reserve(numMaterials);

		for (size_t cur = 0; cur < numMaterials; cur++)
		{
			matFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
				[data, cur, &scene, &sceneRootPath]() 
					-> util::ThreadSafeVector<std::future<void>> {

				util::ThreadSafeVector<std::future<void>> textureFutures;
				textureFutures.reserve(5); // Albedo, met/rough, normal, occlusion, emissive

				cgltf_material const* const material = &data->materials[cur];

				const string matName = material == nullptr ? "MissingMaterial" : GenerateMaterialName(*material);
				if (scene.MaterialExists(matName))
				{
					SEAssertF("We expect all materials in the incoming scene data are unique");
					return textureFutures;
				}

				LOG("Loading material \"%s\"", matName.c_str());

				SEAssert("We currently only support the PBR metallic/roughness material model",
					material->has_pbr_metallic_roughness == 1);

				shared_ptr<Material> newMat =
					make_shared<Material>(matName, Material::GetMaterialDefinition("pbrMetallicRoughness"));

				newMat->SetShader(nullptr); // Not required; just for clarity

				// GLTF specifications: If a texture is not given, all respective texture components are assumed to be 1.f
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
				constexpr vec4 missingTextureColor(1.f, 1.f, 1.f, 1.f);

				// MatAlbedo
				textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
					[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
					newMat->SetTexture(0, LoadTextureOrColor(
						scene,
						sceneRootPath,
						material->pbr_metallic_roughness.base_color_texture.texture,
						missingTextureColor,
						Texture::Format::RGBA8,
						Texture::ColorSpace::sRGB));
					}));

				// MatMetallicRoughness
				textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
					[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
					newMat->SetTexture(1, LoadTextureOrColor(
						scene,
						sceneRootPath,
						material->pbr_metallic_roughness.metallic_roughness_texture.texture,
						missingTextureColor,
						Texture::Format::RGBA8,
						Texture::ColorSpace::Linear));
					}));

				// MatNormal
				textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
					[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
					newMat->SetTexture(2, LoadTextureOrColor(
						scene,
						sceneRootPath,
						material->normal_texture.texture,
						vec4(0.5f, 0.5f, 1.0f, 0.0f), // Equivalent to a [0,0,1] normal after unpacking
						Texture::Format::RGBA8,
						Texture::ColorSpace::Linear));
					}));

				// MatOcclusion
				textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
					[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
					newMat->SetTexture(3, LoadTextureOrColor(
						scene,
						sceneRootPath,
						material->occlusion_texture.texture,
						missingTextureColor,	// Completely unoccluded
						Texture::Format::RGBA8,
						Texture::ColorSpace::Linear));
					}));

				// MatEmissive
				textureFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
					[newMat, &missingTextureColor, &scene, &sceneRootPath, material]() {
					newMat->SetTexture(4, LoadTextureOrColor(
						scene,
						sceneRootPath,
						material->emissive_texture.texture,
						missingTextureColor,
						Texture::Format::RGBA8,
						Texture::ColorSpace::sRGB)); // GLTF convention: Must be converted to linear before use
					}));


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

				newMat->SetParameterBlock(matParams);

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


	void SetTransformValues(cgltf_node* current, Transform* targetTransform)
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

			targetTransform->SetLocalRotation(rotation);
			targetTransform->SetLocalScale(scale);
			targetTransform->SetLocalTranslation(translation);
		}
		else
		{
			if (current->has_rotation)
			{
				// Note: GLM expects quaternions to be specified in WXYZ order
				targetTransform->SetLocalRotation(
					glm::quat(current->rotation[3], current->rotation[0], current->rotation[1], current->rotation[2]));
			}
			if (current->has_scale)
			{
				targetTransform->SetLocalScale(glm::vec3(current->scale[0], current->scale[1], current->scale[2]));
			}
			if (current->has_translation)
			{
				targetTransform->SetLocalTranslation(
					glm::vec3(current->translation[0], current->translation[1], current->translation[2]));
			}
		}
	};


	// Creates a default camera if camera == nullptr, and no cameras exist in scene
	void LoadAddCamera(SceneData& scene, shared_ptr<SceneNode> parent, cgltf_node* current)
	{
		if (parent == nullptr && (current == nullptr || current->camera == nullptr))
		{
			LOG("Creating a default camera");

			gr::Camera::CameraConfig camConfig;
			camConfig.m_aspectRatio = Config::Get()->GetWindowAspectRatio();
			camConfig.m_yFOV = Config::Get()->GetValue<float>("defaultyFOV");
			camConfig.m_near = Config::Get()->GetValue<float>("defaultNear");
			camConfig.m_far = Config::Get()->GetValue<float>("defaultFar");
			camConfig.m_exposure = Config::Get()->GetValue<float>("defaultExposure");

			scene.AddCamera(make_shared<Camera>("Default camera", camConfig, nullptr));

			return;
		}

		cgltf_camera const* const camera = current->camera;

		SEAssert("Must supply a parent and camera pointer", parent != nullptr && camera != nullptr);

		const string camName = camera->name ? string(camera->name) : "Unnamed camera";
		LOG("Loading camera \"%s\"", camName.c_str());

		gr::Camera::CameraConfig camConfig;
		camConfig.m_projectionType = camera->type == cgltf_camera_type_orthographic ? 
			Camera::CameraConfig::ProjectionType::Orthographic : Camera::CameraConfig::ProjectionType::Perspective;
		if (camConfig.m_projectionType == Camera::CameraConfig::ProjectionType::Orthographic)
		{
			camConfig.m_yFOV					= 0;
			camConfig.m_near					= camera->data.orthographic.znear;
			camConfig.m_far						= camera->data.orthographic.zfar;
			camConfig.m_orthoLeftRightBotTop.x	= -camera->data.orthographic.xmag / 2.0f;
			camConfig.m_orthoLeftRightBotTop.y	= camera->data.orthographic.xmag / 2.0f;
			camConfig.m_orthoLeftRightBotTop.z	= -camera->data.orthographic.ymag / 2.0f;
			camConfig.m_orthoLeftRightBotTop.w	= camera->data.orthographic.ymag / 2.0f;
		}
		else
		{
			camConfig.m_yFOV					= camera->data.perspective.yfov;
			camConfig.m_near					= camera->data.perspective.znear;
			camConfig.m_far						= camera->data.perspective.zfar;
			camConfig.m_aspectRatio				= camera->data.perspective.has_aspect_ratio ?
				camera->data.perspective.aspect_ratio : 1.0f;
			camConfig.m_orthoLeftRightBotTop.x	= 0.f;
			camConfig.m_orthoLeftRightBotTop.y	= 0.f;
			camConfig.m_orthoLeftRightBotTop.z	= 0.f;
			camConfig.m_orthoLeftRightBotTop.w	= 0.f;
		}

		// Create the camera and set the transform values on the parent object:
		shared_ptr<Camera> newCam = make_shared<Camera>(camName, camConfig, parent->GetTransform());
		SetTransformValues(current, parent->GetTransform());
		
		scene.AddCamera(newCam);
	}


	void LoadAddLight(SceneData& scene, cgltf_node* current, shared_ptr<SceneNode> parent)
	{
		string lightName;
		if (current->light->name)
		{
			lightName = string(current->light->name);
		}
		else
		{
			static std::atomic<uint32_t> unnamedLightIndex = 0;
			const uint32_t thisLightIndex = unnamedLightIndex.fetch_add(1);
			lightName = "UnnamedLight_" + to_string(thisLightIndex);
		}

		LOG("Found light \"%s\"", lightName.c_str());

		Light::LightType lightType = Light::LightType::Directional;
		switch (current->light->type)
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

		const vec3 colorIntensity = glm::make_vec3(current->light->color) * current->light->intensity;
		bool attachShadow = true;
		if (colorIntensity.r + colorIntensity.g + colorIntensity.b == 0.f)
		{
			LOG_WARNING("Light \"%s\" has 0 intensity. Disabling its shadow map, as it will not contribute any energy "
				"to the scene", lightName.c_str());
			attachShadow = false; // No point rendering shadows for non-contributing lights
		}
		
		shared_ptr<Light> newLight = 
			make_shared<Light>(lightName, parent->GetTransform(), lightType, colorIntensity, attachShadow);

		scene.AddLight(newLight);
	}


	void LoadIBL(string const& sceneRootPath, SceneData& scene)
	{
		// Ambient lights are not supported by GLTF 2.0; Instead, we handle it manually.
		// First, we check for a <sceneRoot>\IBL\ibl.hdr file for per-scene IBLs/skyboxes.
		// If that fails, we fall back to a default HDRI
		shared_ptr<Texture> iblTexture = nullptr;

		auto TryLoadIBL = [&scene](std::string const& IBLPath, std::shared_ptr<re::Texture>& iblTexture) {
			if (scene.TextureExists(IBLPath))
			{
				iblTexture = scene.GetTexture(IBLPath);
			}
			else
			{
				iblTexture = LoadTextureFromFilePath({ IBLPath }, false, k_errorTextureColor, re::Texture::ColorSpace::Linear);
			}
		};

		string IBLPath;
		if (Config::Get()->TryGetValue<string>("sceneIBLPath", IBLPath))
		{
			TryLoadIBL(IBLPath, iblTexture);
		}		
		
		if (!iblTexture)
		{
			IBLPath = Config::Get()->GetValue<string>("defaultIBLPath");
			TryLoadIBL(IBLPath, iblTexture);
		}
		SEAssert("Missing IBL texture. Per scene IBLs must be placed at <sceneRoot>\\IBL\\ibl.hdr; A default fallback "
			"must exist at Assets\\DefaultIBL\\ibl.hdr", iblTexture != nullptr);
	}


	void LoadMeshGeometry(
		string const& sceneRootPath, SceneData& scene, cgltf_node* current, shared_ptr<SceneNode> parent)
	{
		string meshName;
		if (current->mesh->name)
		{
			meshName = string(current->mesh->name);
		}
		else
		{
			static std::atomic<uint32_t> unnamedMeshIdx = 0;
			const uint32_t thisMeshIdx = unnamedMeshIdx.fetch_add(1);
			meshName = "UnnamedMesh_" + to_string(thisMeshIdx);
		}

		std::shared_ptr<gr::Mesh> newMesh = make_shared<gr::Mesh>(meshName, parent->GetTransform());

		// Add each MeshPrimitive as a child of the SceneNode's Mesh:
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
			vec3 positionsMinXYZ(Bounds::k_invalidMinXYZ);
			vec3 positionsMaxXYZ(Bounds::k_invalidMaxXYZ);
			vector<float> normals;
			vector<float> tangents;
			vector<float> uv0;
			bool foundUV0 = false; // TODO: Support minimum of 2 UV sets. For now, just use the 1st
			vector<float> colors;
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
						SEAssert("Unexpected number of bytes in min value array data",
							sizeof(current->mesh->primitives[primitive].attributes[attrib].data->min) == 64);

						float* xyzComponent = current->mesh->primitives[primitive].attributes[attrib].data->min;
						positionsMinXYZ.x = *xyzComponent++;
						positionsMinXYZ.y = *xyzComponent++;
						positionsMinXYZ.z = *xyzComponent;
					}
					if (current->mesh->primitives[primitive].attributes[attrib].data->has_max)
					{
						SEAssert("Unexpected number of bytes in max value array data",
							sizeof(current->mesh->primitives[primitive].attributes[attrib].data->max) == 64);

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
					SEAssert("Only 4-channel colors (RGBA) are currently supported", elementsPerComponent == 4);
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
				SEAssert("Failed to unpack data", unpackResult);

				// Post-process the data:
				if (attributeType == cgltf_attribute_type_joints)
				{
					// Cast our joint indexes from floats to uint8_t's:
					SEAssert("Source/destination size mismatch", jointsAsFloats.size() == jointsAsUints.size());
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
				reinterpret_cast<vector<vec3>*>(&positions),
				reinterpret_cast<vector<vec3>*>(&normals),
				reinterpret_cast<vector<vec4>*>(&tangents),
				reinterpret_cast<vector<vec2>*>(&uv0),
				reinterpret_cast<vector<vec4>*>(&colors),
				reinterpret_cast<vector<glm::tvec4<uint8_t>>*>(&jointsAsUints),
				reinterpret_cast<vector<vec4>*>(&weights)
			};
			util::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

			// Assign a material:
			shared_ptr<gr::Material> material;
			if (current->mesh->primitives[primitive].material != nullptr)
			{
				const string generatedMatName = GenerateMaterialName(*current->mesh->primitives[primitive].material);
				material = scene.GetMaterial(generatedMatName);
			}
			else
			{
				LOG_WARNING("MeshPrimitive \"%s\" does not have a material. Assigning \"%s\"", 
					meshName.c_str(), k_missingMaterialName);
				material = scene.GetMaterial(k_missingMaterialName);
			}

			// Attach the MeshPrimitive to the Mesh:
			newMesh->AddMeshPrimitive(MeshPrimitive::Create(
				meshName,
				indices,
				positions,
				positionsMinXYZ,
				positionsMaxXYZ,
				normals,
				tangents,
				uv0,
				colors,
				jointsAsUints,
				weights,
				material,
				meshPrimitiveParams));
		}

		// Finally, register the mesh with the scene
		scene.AddMesh(newMesh);
	}


	void LoadObjectHierarchyRecursiveHelper(
		string const& sceneRootPath, 
		SceneData& scene, 
		cgltf_data* data, 
		cgltf_node* current, 
		shared_ptr<SceneNode> parent, 
		std::vector<std::future<void>>& loadTasks)
	{
		if (current == nullptr)
		{
			SEAssertF("We should not be traversing into null nodes");
			return;
		}

		SEAssert("TODO: Handle nodes with multiple things (eg. Light & Mesh) that depend on a transform",
			current->light == nullptr || current->mesh == nullptr);
		// TODO: Seems we never hit this... Does GLTF support multiple attachments per node?

		if (current->children_count > 0) // Depth-first traversal
		{
			for (size_t i = 0; i < current->children_count; i++)
			{
				shared_ptr<SceneNode> childNode = make_shared<SceneNode>(parent->GetTransform());

				LoadObjectHierarchyRecursiveHelper(
					sceneRootPath, scene, data, current->children[i], childNode, loadTasks);
			}
		}

		// Set the SceneNode transform:
		loadTasks.emplace_back(CoreEngine::GetThreadPool()->EnqueueJob([current, parent]()
		{
			SetTransformValues(current, parent->GetTransform());
		}));
		
		// Process node attachments:
		if (current->mesh)
		{
			loadTasks.emplace_back(CoreEngine::GetThreadPool()->EnqueueJob([&sceneRootPath, &scene, current, parent]()
			{
				LoadMeshGeometry(sceneRootPath, scene, current, parent);
			}));
		}
		if (current->light)
		{
			loadTasks.emplace_back(CoreEngine::GetThreadPool()->EnqueueJob([&scene, current, parent]()
			{
				LoadAddLight(scene, current, parent);
			}));
		}
		if (current->camera)
		{
			loadTasks.emplace_back(CoreEngine::GetThreadPool()->EnqueueJob([&scene, current, parent]()
			{
				LoadAddCamera(scene, parent, current);
			}));
		}

		scene.AddSceneNode(parent);
	}


	// Note: data must already be populated by calling cgltf_load_buffers
	void LoadSceneHierarchy(std::string const& sceneRootPath, fr::SceneData& scene, cgltf_data* data)
	{
		LOG("Scene has %d object nodes", data->nodes_count);

		SEAssert("Loading > 1 scene is currently unsupported", data->scenes_count == 1);

		std::vector<std::future<void>> loadTasks; // Task enqueuing is single-threaded

		// Each node is the root in a transformation hierarchy:
		for (size_t node = 0; node < data->scenes->nodes_count; node++)
		{
			SEAssert("Error: Node is not a root", data->scenes->nodes[node]->parent == nullptr);

			LOG("Loading root node %zu: \"%s\"", node, data->scenes->nodes[node]->name);

			shared_ptr<SceneNode> currentNode = make_shared<SceneNode>(nullptr); // Root node has no parent

			LoadObjectHierarchyRecursiveHelper(
				sceneRootPath, scene, data, data->scenes->nodes[node], currentNode, loadTasks);
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
	bool SceneData::Load(string const& sceneFilePath)
	{
		std::future<void> errorMatTaskFuture = 
			en::CoreEngine::GetThreadPool()->EnqueueJob([this]() {
				GenerateErrorMaterial(*this);
			});


		string sceneRootPath;
		Config::Get()->TryGetValue<string>("sceneRootPath", sceneRootPath);

		const bool gotSceneFilePath = !sceneFilePath.empty();
		if (gotSceneFilePath)
		{
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
			m_textures.reserve(max((int)data->textures_count, 10));
			m_materials.reserve(max((int)data->materials_count, 10));
			m_pointLights.reserve(max((int)data->lights_count, 10)); // Probably an over-estimation
			m_cameras.reserve(max((int)data->cameras_count, 5));

			
			// Load the materials first:
			PreLoadMaterials(sceneRootPath, *this, data);

			// Load the scene hierarchy:
			LoadSceneHierarchy(sceneRootPath, *this, data);

			// Cleanup:
			cgltf_free(data);
		}

		// Load the IBL/skybox HDRI:
		LoadIBL(sceneRootPath, *this); // TODO: Enqueue this onto a worker thread while we're loading other stuff

		if (m_cameras.empty()) // Add a default camera if none were found during LoadSceneHierarchy()
		{
			LoadAddCamera(*this, nullptr, nullptr);
		}

		errorMatTaskFuture.wait();

		return true;
	}


	void SceneData::PostLoadFinalize()
	{
		m_finishedLoading = true;
	}


	SceneData::SceneData(string const& sceneName)
		: NamedObject(sceneName)
		, m_ambientLight(nullptr)
		, m_keyLight(nullptr)
		, m_finishedLoading(false)
	{
	}


	void SceneData::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_updateablesMutex);
			m_updateables.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_sceneNodesMutex);
			m_sceneNodes.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_meshesAndMeshPrimitivesMutex);
			m_meshPrimitives.clear();
			m_meshes.clear();
		}
		{
			std::lock_guard<std::shared_mutex> lock(m_texturesMutex);
			m_textures.clear();
		}
		{
			std::lock_guard<std::shared_mutex> lock(m_materialsMutex);
			m_materials.clear();
		}
		{
			std::lock_guard<std::shared_mutex> lock(m_shadersMutex);
			m_shaders.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_ambientLightMutex);
			m_ambientLight = nullptr;
		}
		{
			std::lock_guard<std::mutex> lock(m_keyLightMutex);
			m_keyLight = nullptr;
		}
		{
			std::lock_guard<std::mutex> lock(m_pointLightsMutex);
			m_pointLights.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_camerasMutex);
			m_cameras.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_sceneBoundsMutex);
			m_sceneWorldSpaceBounds = Bounds();
		}
	}


	void SceneData::AddCamera(std::shared_ptr<gr::Camera> newCamera)
	{
		SEAssert("Cannot add a null camera", newCamera != nullptr);
		{
			std::lock_guard<std::mutex> lock(m_camerasMutex);
			m_cameras.emplace_back(newCamera);
		}

		AddUpdateable(newCamera);
	}


	void SceneData::AddLight(std::shared_ptr<Light> newLight)
	{
		// TODO: Seems arbitrary that we cannot have multiple directional lights... Why even bother 
		// enforcing this? Just treat all lights the same

		switch (newLight->Type())
		{
		case Light::AmbientIBL:
		{
			std::lock_guard<std::mutex> lock(m_ambientLightMutex);
			SEAssert("Ambient light already exists, cannot have 2 ambient lights", m_ambientLight == nullptr);
			m_ambientLight = newLight;
		}
		break;
		case Light::Directional:
		{
			std::lock_guard<std::mutex> lock(m_keyLightMutex);
			SEAssert("Direction light already exists, cannot have 2 directional lights", m_keyLight == nullptr);
			m_keyLight = newLight;
		}
		break;
		case Light::Point:
		{
			std::lock_guard<std::mutex> lock(m_pointLightsMutex);
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

		AddUpdateable(newLight); // Updateables get pumped every frame
	}


	std::shared_ptr<re::Texture> SceneData::GetIBLTexture() const
	{
		shared_ptr<Texture> iblTexture = nullptr;
		string sceneIBLPath;
		bool result = Config::Get()->TryGetValue<string>("sceneIBLPath", sceneIBLPath);
		if (result)
		{
			iblTexture = TryGetTexture(sceneIBLPath);
		}
		
		if (!iblTexture)
		{
			const string defaultIBLPath = Config::Get()->GetValue<string>("defaultIBLPath");
			iblTexture = GetTexture(defaultIBLPath); // Will exist
		}

		return iblTexture;
	}


	void SceneData::AddMesh(std::shared_ptr<gr::Mesh> mesh)
	{
		SEAssert("Adding data is not thread safe once loading is complete", !m_finishedLoading);

		// Only need to hold the lock while we modify m_meshes and m_meshPrimitives
		{
			std::lock_guard<std::mutex> lock(m_meshesAndMeshPrimitivesMutex);

#if defined(_DEBUG)
			for (size_t i = 0; i < mesh->GetMeshPrimitives().size(); i++)
			{
				const uint64_t meshPrimitiveDataHash = mesh->GetMeshPrimitives()[i]->GetDataHash();

				SEAssert("We expect the MeshPrimitives have already been registered",
					m_meshPrimitives.find(meshPrimitiveDataHash) != m_meshPrimitives.end());
			}
#endif

			m_meshes.emplace_back(mesh); // Add the mesh to our tracking list
		}
		UpdateSceneBounds(mesh);
	}


	bool SceneData::AddUniqueMeshPrimitive(std::shared_ptr<re::MeshPrimitive>& meshPrimitive)
	{
		const uint64_t meshPrimitiveDataHash = meshPrimitive->GetDataHash();
		bool replacedIncomingPtr = false;
		{
			std::lock_guard<std::mutex> lock(m_meshesAndMeshPrimitivesMutex);

			auto const& result = m_meshPrimitives.find(meshPrimitiveDataHash);
			if (result != m_meshPrimitives.end())
			{
				LOG("MeshPrimitive \"%s\" has the same data hash as an existing MeshPrimitive. It will be replaced "
					"with a shared copy",
					meshPrimitive->GetName().c_str());

				meshPrimitive = result->second;
				replacedIncomingPtr = true;
			}
			else
			{
				m_meshPrimitives.insert({ meshPrimitiveDataHash, meshPrimitive });
			}
		}
		return replacedIncomingPtr;
	}


	std::vector<std::shared_ptr<gr::Mesh>> const& SceneData::GetMeshes() const
	{
		SEAssert("Accessing data container is not thread safe during loading", m_finishedLoading);
		return m_meshes;
	}


	std::vector<std::shared_ptr<gr::Camera>> const& SceneData::GetCameras() const 
	{
		SEAssert("Accessing data container is not thread safe during loading", m_finishedLoading);
		return m_cameras;
	}


	std::shared_ptr<gr::Camera> SceneData::GetMainCamera() const
	{
		// TODO: This camera is accessed multiple times before the first frame is rendered (e.g. PlayerObject, various
		// graphics systems). Currently, this is fine as we currently join any loading threads before creating these
		// objects, but it may not always be the case.

		/*SEAssert("Accessing this data is not thread safe during loading", m_finishedLoading);*/
		return m_cameras.at(0);
	}

	gr::Bounds const& SceneData::GetWorldSpaceSceneBounds() const 
	{
		SEAssert("Accessing this data is not thread safe during loading", m_finishedLoading);
		return m_sceneWorldSpaceBounds;
	}


	std::vector<std::shared_ptr<en::Updateable>> const& SceneData::GetUpdateables() const
	{
		SEAssert("Accessing data container is not thread safe during loading", m_finishedLoading);
		return m_updateables;
	}


	void SceneData::AddUpdateable(std::shared_ptr<en::Updateable> updateable)
	{
		std::lock_guard<std::mutex> lock(m_updateablesMutex);
		m_updateables.emplace_back(updateable);
	}


	void SceneData::AddSceneNode(std::shared_ptr<fr::SceneNode> sceneNode)
	{
		std::lock_guard<std::mutex> lock(m_sceneNodesMutex);
		m_sceneNodes.emplace_back(sceneNode);
	}


	void SceneData::UpdateSceneBounds(std::shared_ptr<gr::Mesh> mesh)
	{
		std::lock_guard<std::mutex> lock(m_sceneBoundsMutex);

		m_sceneWorldSpaceBounds.ExpandBounds(
			mesh->GetBounds().GetTransformedAABBBounds(mesh->GetTransform()->GetGlobalMatrix(Transform::TRS)));
	}


	void SceneData::RecomputeSceneBounds()
	{
		SEAssert("This function should be called during the main loop only", m_finishedLoading);

		// Walk down our Transform hierarchy, recomputing each Transform in turn (just for efficiency). This also has
		// the benefit that our Transforms will be up to date when we copy them for the Render thread
		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(m_sceneNodes.size());
		for (shared_ptr<fr::SceneNode> root : m_sceneNodes)
		{
			taskFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
				[root]() 
				{
					std::stack<gr::Transform*> transforms;
					transforms.push(root->GetTransform());

					while (!transforms.empty())
					{
						transforms.top()->Recompute();
						transforms.pop();

						for (gr::Transform* child : root->GetTransform()->GetChildren())
						{
							transforms.push(child);
						}
					}
				}));
		}
		for (std::future<void> const& taskFuture : taskFutures)
		{
			taskFuture.wait();
		}

		// Now all of our transforms are clean, update the scene bounds:
		std::unique_lock<std::mutex> lock(m_sceneBoundsMutex);
		m_sceneWorldSpaceBounds = gr::Bounds();
		for (shared_ptr<gr::Mesh> mesh : m_meshes)
		{
			m_sceneWorldSpaceBounds.ExpandBounds(
				mesh->GetBounds().GetTransformedAABBBounds(mesh->GetTransform()->GetGlobalMatrix(Transform::TRS)));
		}
	}


	bool SceneData::AddUniqueTexture(shared_ptr<re::Texture>& newTexture)
	{
		SEAssert("Adding data is not thread safe once loading is complete", !m_finishedLoading);
		SEAssert("Cannot add null texture to textures table", newTexture != nullptr);

		std::unique_lock<std::shared_mutex> writeLock(m_texturesMutex);

		unordered_map<size_t, shared_ptr<re::Texture>>::const_iterator texturePosition =
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


	std::shared_ptr<re::Texture> SceneData::GetTexture(std::string const& texName) const
	{
		const uint64_t nameID = en::NamedObject::ComputeIDFromName(texName);

		std::shared_lock<std::shared_mutex> readLock(m_texturesMutex);
		auto result = m_textures.find(nameID);

		SEAssert("Texture with that name does not exist", result != m_textures.end());

		return result->second;
	}


	std::shared_ptr<re::Texture> SceneData::TryGetTexture(std::string const& texName) const
	{
		const uint64_t nameID = en::NamedObject::ComputeIDFromName(texName);

		std::shared_lock<std::shared_mutex> readLock(m_texturesMutex);
		auto result = m_textures.find(nameID);

		return result == m_textures.end() ? nullptr : result->second;
	}


	bool SceneData::TextureExists(std::string const& textureName) const
	{
		const uint64_t nameID = en::NamedObject::ComputeIDFromName(textureName);

		std::shared_lock<std::shared_mutex> readLock(m_texturesMutex);
		return m_textures.find(nameID) != m_textures.end();
	}


	void SceneData::AddUniqueMaterial(shared_ptr<Material>& newMaterial)
	{
		SEAssert("Adding data is not thread safe once loading is complete", !m_finishedLoading);
		SEAssert("Cannot add null material to material table", newMaterial != nullptr);

		std::unique_lock<std::shared_mutex> writeLock(m_materialsMutex);

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


	std::shared_ptr<gr::Material> SceneData::GetMaterial(std::string const& materialName) const
	{
		const size_t nameID = NamedObject::ComputeIDFromName(materialName);

		std::shared_lock<std::shared_mutex> readLock(m_materialsMutex);
		unordered_map<size_t, shared_ptr<gr::Material>>::const_iterator matPos = m_materials.find(nameID);

		SEAssert("Could not find material", matPos != m_materials.end());

		return matPos->second;
	}


	bool SceneData::MaterialExists(std::string const& matName) const
	{
		const size_t nameID = NamedObject::ComputeIDFromName(matName);

		std::shared_lock<std::shared_mutex> readLock(m_materialsMutex);

		return m_materials.find(nameID) != m_materials.end();
	}


	void SceneData::AddUniqueShader(std::shared_ptr<re::Shader>& newShader)
	{
		SEAssert("Cannot add null shader to shader table", newShader != nullptr);

		std::unique_lock<std::shared_mutex> writeLock(m_shadersMutex);

		// Note: Materials are uniquely identified by name, regardless of the MaterialDefinition they might use
		unordered_map<size_t, shared_ptr<re::Shader>>::const_iterator shaderPosition =
			m_shaders.find(newShader->GetNameID());
		if (shaderPosition != m_shaders.end()) // Found existing
		{
			newShader = shaderPosition->second;
		}
		else // Add new
		{
			m_shaders[newShader->GetNameID()] = newShader;
			LOG("Shader \"%s\" registered with scene", newShader->GetName().c_str());
		}
	}


	std::shared_ptr<re::Shader> SceneData::GetShader(std::string const& extensionlessShaderFilename) const
	{
		const size_t filenameID = NamedObject::ComputeIDFromName(extensionlessShaderFilename);

		std::shared_lock<std::shared_mutex> readLock(m_shadersMutex);
		unordered_map<size_t, shared_ptr<re::Shader>>::const_iterator shaderPos = m_shaders.find(filenameID);

		SEAssert("Could not find shader", shaderPos != m_shaders.end());

		return shaderPos->second;
	}


	bool SceneData::ShaderExists(std::string const& extensionlessShaderFilename) const
	{
		const size_t filenameID = NamedObject::ComputeIDFromName(extensionlessShaderFilename);

		std::shared_lock<std::shared_mutex> readLock(m_shadersMutex);

		return m_materials.find(filenameID) != m_materials.end();
	}
}