// © 2023 Adam Badke. All rights reserved.

// Note: We can't include STBI in our pch, as the following define can only be included ONCE in the project
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#include "PerformanceTimer.h"
#include "Texture.h"
#include "TextureLoadUtils.h"


namespace util
{
	re::Texture::ImageDataUniquePtr CreateImageDataUniquePtr(void* imageData)
	{
		re::Texture::ImageDataUniquePtr imageDataPtr(
			imageData,
			[](void* stbImageData) { stbi_image_free(stbImageData); });

		return std::move(imageDataPtr);
	};


	std::shared_ptr<re::Texture> LoadTextureFromFilePath(
		std::vector<std::string> texturePaths,
		bool returnErrorTex,
		glm::vec4 const& errorTexFillColor,
		re::Texture::ColorSpace colorSpace)
	{
		SEAssert("Can load single faces or cubemaps only", texturePaths.size() == 1 || texturePaths.size() == 6);
		SEAssert("Invalid number of texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);

		LOG("Attempting to load %d texture(s): \"%s\"...", texturePaths.size(), texturePaths[0].c_str());

		PerformanceTimer timer;
		timer.Start();

		const uint32_t totalFaces = (uint32_t)texturePaths.size();

		// Modify default TextureParams to be suitable for a generic error texture:
		re::Texture::TextureParams texParams
		{
			.m_faces = totalFaces,

			.m_usage = re::Texture::Usage::Color,
			.m_dimension = (totalFaces == 1 ?
				re::Texture::Dimension::Texture2D : re::Texture::Dimension::TextureCubeMap),
			.m_format = re::Texture::Format::RGBA8,
			.m_colorSpace = colorSpace
		};
		glm::vec4 fillColor = errorTexFillColor;

		// Load the texture, face-by-face:
		std::vector<re::Texture::ImageDataUniquePtr> initialData;
		std::shared_ptr<re::Texture> texture = nullptr;
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

				initialData.emplace_back(CreateImageDataUniquePtr(imageData));

				if (face == 0) // 1st face: Update the texture parameters
				{
					texParams.m_width = width;
					texParams.m_height = height;

					if ((width == 1 || height == 1) && (width != height))
					{
						SEAssertF("Found 1D texture, but 1D textures are currently not supported. Treating "
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

					fillColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color
				}
				else // texture already exists: Ensure the face has the same dimensions
				{
					SEAssert("Parameter mismatch", texParams.m_width == width && texParams.m_height == height);
				}
			}
			else if (returnErrorTex)
			{
				if (!initialData.empty())
				{
					initialData.clear();

					// Reset texParams to be suitable for an error texture
					texParams.m_width = 2;
					texParams.m_height = 2;
					texParams.m_dimension = totalFaces == 1 ?
						re::Texture::Dimension::Texture2D : re::Texture::Dimension::TextureCubeMap;
					texParams.m_format = re::Texture::Format::RGBA8;
					texParams.m_colorSpace = re::Texture::ColorSpace::sRGB;
					texParams.m_mipMode = re::Texture::MipMode::AllocateGenerate;

					fillColor = errorTexFillColor;
				}

				// We'll populate the initial image data internally:
				texture = re::Texture::Create(texturePaths[0], texParams, true, fillColor);
				break;
			}
			else
			{
				char const* failResult = stbi_failure_reason();
				LOG_WARNING("Failed to load image \"%s\": %s", texturePaths[0].c_str(), failResult);
				timer.StopSec();
				return nullptr;
			}
		}

		if (!texture)
		{
			texture = re::Texture::Create(
				texturePaths[0], texParams, false, glm::vec4(0.f, 0.f, 0.f, 1.f), std::move(initialData));
		}

		LOG("Loaded texture \"%s\" in %f seconds...", texturePaths[0].c_str(), timer.StopSec());

		// Note: Texture color space must still be set
		return texture;
	}


	std::shared_ptr<re::Texture> LoadTextureFromMemory(
		std::string const& texName,
		unsigned char const* texSrc,
		uint32_t texSrcNumBytes,
		re::Texture::ColorSpace colorSpace)
	{
		SEAssert("Invalid texture memory allocation", texSrc != nullptr && texSrcNumBytes > 0);

		LOG("Attempting to load texture \"%s\" from memory...", texName.c_str());
		PerformanceTimer timer;
		timer.Start();

		// Modify default TextureParams to be suitable for a generic error texture:
		re::Texture::TextureParams texParams
		{
			.m_usage = re::Texture::Usage::Color,
			.m_dimension = re::Texture::Dimension::Texture2D,
			.m_format = re::Texture::Format::RGBA8,
			.m_colorSpace = colorSpace
		};
		glm::vec4 fillColor = re::Texture::k_errorTextureColor;

		// Get the image data:
		int width = 0;
		int height = 0;
		int numChannels = 0;
		stbi_info_from_memory(static_cast<stbi_uc const*>(texSrc), texSrcNumBytes, &width, &height, &numChannels);

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

		std::shared_ptr<re::Texture> texture(nullptr);
		std::vector<re::Texture::ImageDataUniquePtr> initialData;
		if (imageData)
		{
			LOG("Texture \"%s\" is %dx%d, %d-bit, %d channels",
				texName.c_str(), width, height, bitDepth, desiredChannels);

			initialData.emplace_back(std::move(CreateImageDataUniquePtr(imageData)));

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

			// Create the texture now the params are configured:
			fillColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color

			texture = re::Texture::Create(texName, texParams, false, fillColor, std::move(initialData));
		}
		else
		{
			SEAssertF("Failed to load image data");
		}

		LOG("Loaded texture \"%s\" from memory in %f seconds...", texName.c_str(), timer.StopSec());

		// Note: Texture color space must still be set
		return texture;
	}


	std::string GenerateTextureColorFallbackName(
		glm::vec4 const& colorFallback, size_t numChannels, re::Texture::ColorSpace colorSpace)
	{
		std::string texName = "Color_" + std::to_string(colorFallback.x) + "_";
		if (numChannels >= 2)
		{
			texName += std::to_string(colorFallback.y) + "_";
			if (numChannels >= 3)
			{
				texName += std::to_string(colorFallback.z) + "_";
				if (numChannels >= 4)
				{
					texName += std::to_string(colorFallback.w) + "_";
				}
			}
		}
		texName += (colorSpace == re::Texture::ColorSpace::sRGB ? "sRGB" : "Linear");

		return texName;
	}


	// Assemble a name for textures loaded from memory: Either use the provided name, or create a unique one
	std::string GenerateEmbeddedTextureName(char const* texName)
	{
		std::string texNameStr;

		if (texName != nullptr)
		{
			texNameStr = std::string(texName);
		}
		else
		{
			static std::atomic<uint32_t> unnamedTexIdx = 0;
			const uint32_t thisTexIdx = unnamedTexIdx.fetch_add(1);
			texNameStr = "EmbeddedTexture_" + std::to_string(thisTexIdx);
		}

		return texNameStr;
	}
}