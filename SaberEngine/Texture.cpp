#include <string>

#define STBI_FAILURE_USERMSG
#include <stb_image.h>	// STB image loader. No need to #define STB_IMAGE_IMPLEMENTATION, as it was already defined in SceneManager

#include "Texture.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Material.h"

using gr::Material;
using std::to_string;
using std::string;
using glm::vec4;

#define ERROR_TEXTURE_NAME "ErrorTexture"
#define ERROR_TEXTURE_COLOR_VEC4 vec4(1.0f, 0.0f, 1.0f, 1.0f)
#define DEFAULT_ALPHA_VALUE 1.0f			// Default alpha value when loading texture data, if no alpha exists


namespace
{
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
}


namespace gr
{
	Texture::Texture(TextureParams params) :
		m_texParams{ params },
		m_texels(params.m_faces * params.m_width * params.m_height, params.m_clearColor),
		m_isCreated{ false },
		m_isDirty{ true },
		m_platformParams{ nullptr } // Initialized during Create(), to ensure the texture is correclty configured
	{
	}


	void gr::Texture::Create()
	{
		// Note: Textures are shared, so duplicate Create() calls can/do happen. Simplest solution is to just abort here
		if (m_isCreated)
		{
			return;
		}

		platform::Texture::Create(*this);
		m_isDirty = false;
		m_isCreated = true;

		return;
	}


	void gr::Texture::Bind(uint32_t textureUnit, bool doBind) const
	{
		platform::Texture::Bind(*this, textureUnit, doBind);
	}


	void Texture::Destroy()
	{
		if (m_texels.size() > 0)
		{
			m_texels.clear();
		}
		m_isCreated = false;
		m_isDirty = true;

		platform::Texture::Destroy(*this);

		m_platformParams = nullptr;
	}


	vec4 const& gr::Texture::GetTexel(uint32_t u, uint32_t v, uint32_t faceIdx/*= 0*/) const
	{
		SEAssert("OOB texel coordinates",
			u >= 0 && 
			u < m_texParams.m_width && 
			v >= 0 && 
			v < m_texParams.m_height && 
			faceIdx < m_texParams.m_faces);

		// Number of elements in v rows, + uth element in next row
		return m_texels[(faceIdx * m_texParams.m_width * m_texParams.m_height) + (v * m_texParams.m_width) + u];
	}


	glm::vec4 const& gr::Texture::GetTexel(uint32_t index) const
	{
		SEAssert("OOB texel coordinates", index < (m_texParams.m_faces * m_texParams.m_width * m_texParams.m_height));
		return m_texels[index];
	}


	void gr::Texture::SetTexel(uint32_t u, uint32_t v, glm::vec4 value)
	{
		SEAssert("OOB texel coordinates", u >= 0 && u < m_texParams.m_width&& v >= 0 && v < m_texParams.m_height);

		m_texels[(v * m_texParams.m_width) + u] = value;
		m_isDirty = true;
	}


	void gr::Texture::Fill(vec4 solidColor)
	{
		for (uint32_t row = 0; row < m_texParams.m_height; row++)
		{
			for (uint32_t col = 0; col < m_texParams.m_width; col++)
			{
				SetTexel(row, col, solidColor);
			}
		}
		m_isDirty = true;
	}


	void gr::Texture::Fill(vec4 tl, vec4 tr, vec4 bl, vec4 br)
	{
		for (unsigned int row = 0; row < m_texParams.m_height; row++)
		{
			float vertDelta = (float)((float)row / (float)m_texParams.m_height);
			vec4 startCol = (vertDelta * bl) + ((1.0f - vertDelta) * tl);
			vec4 endCol = (vertDelta * br) + ((1.0f - vertDelta) * tr);

			for (unsigned int col = 0; col < m_texParams.m_width; col++)
			{
				float horDelta = (float)((float)col / (float)m_texParams.m_width);

				SetTexel(row, col, (horDelta * endCol) + ((1.0f - horDelta) * startCol));
			}
		}
		m_isDirty = true;
	}


	// Static functions:
	//------------------

	bool gr::Texture::LoadTextureFileFromPath(
		std::shared_ptr<gr::Texture>& texture,
		string texturePath, 
		TextureColorSpace colorSpace,
		bool returnErrorTexIfNotFound /*= false*/,		
		uint32_t totalFaces /*= 1*/,
		size_t faceIndex /*= 0*/)
	{
		LOG("Attempting to load texture \"" + texturePath + "\"");

		// Flip the y-axis on loading (so pixel (0,0) is in the bottom-left of the image if using OpenGL
		platform::RenderingAPI const& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();
		const bool flipY = api == platform::RenderingAPI::OpenGL ? true : false;

		stbi_set_flip_vertically_on_load(flipY);	

		// Get the image data:
		int width, height, numChannels;
		void* imageData = nullptr;
		size_t bitDepth = 0;

		if (stbi_is_hdr(texturePath.c_str()))	// HDR
		{
			imageData	= stbi_loadf(texturePath.c_str(), &width, &height, &numChannels, 0);
			bitDepth = 32;
		}
		else if (stbi_is_16_bit(texturePath.c_str()))
		{
			// TODO: Support loading 16 bit images
			LOG_WARNING("Loading 16 bit image as 8 bit");
			imageData = stbi_load(texturePath.c_str(), &width, &height, &numChannels, 0);
			//imageData = stbi_load_16(texturePath.c_str(), &width, &height, &numChannels, 0);
			bitDepth = 16;
		}
		else // Non-HDR
		{
			imageData = stbi_load(texturePath.c_str(), &width, &height, &numChannels, 0);
			bitDepth = 8;
		}
		
		// Default parameters for an error texture:
		TextureParams texParams;
		if (texture == nullptr)
		{
			texParams.m_width = 1;
			texParams.m_height = 1;
			texParams.m_faces = totalFaces;
			texParams.m_texUse = gr::Texture::TextureUse::Color;
			
			// TODO: This check will need to be changed if we support array textures
			texParams.m_texDimension = 
				totalFaces == 1 ? gr::Texture::TextureDimension::Texture2D : gr::Texture::TextureDimension::TextureCubeMap;

			texParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
			// Use defaults for color space, sampler, min/maxification
			texParams.m_clearColor = ERROR_TEXTURE_COLOR_VEC4;
			texParams.m_texturePath = ERROR_TEXTURE_NAME;
		}
		else
		{
			texParams = texture->GetTextureParams();
		}		

		if (imageData)
		{
			LOG("Found " + to_string(width) + "x" + to_string(height) + ", " + std::to_string(bitDepth) +
				"-bit texture with " + to_string(numChannels) + " channels");

			if (texture == nullptr)
			{
				// Update the texture parameters:
				texParams.m_width = width;
				texParams.m_height = height;
				texParams.m_faces = totalFaces;
				texParams.m_texUse = gr::Texture::TextureUse::Color; // Assume this is a color source

				if ((width == 1 || height == 1) && (width != height))
				{
					LOG_WARNING("Found 1D texture, but 1D textures are currently not supported. Treating "
						"this texture as 2D");
					texParams.m_texDimension = gr::Texture::TextureDimension::Texture2D; // TODO: Replace this
					/*texParams.m_texDimension = gr::Texture::TextureDimension::Texture1D;*/

					// TODO: This won't work if we support texture arrays
				}

				// Currently, we force-pack everything into a 4-channel, 32-bit RGBA texture (in our LDR/HDR helpers).
				// TODO: Support arbitrary texture layouts
				texParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F;

				texParams.m_texColorSpace = colorSpace;
				texParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color
				texParams.m_texturePath = texturePath;

				texture = std::make_shared<gr::Texture>(texParams);
			}		

			const size_t firstTexelIndex = faceIndex * width * height;

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

			#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
				SaberEngine::LOG("Completed loading texture: " + m_texturePath);
			#endif

			return true;
		}


		// If we've made it this far, the texture failed to load
		char const* failResult = stbi_failure_reason();

		if (!returnErrorTexIfNotFound)
		{
			LOG_ERROR("Could not load texture at \"" + texturePath + "\", error: \"" + string(failResult) + 
				".\" Received \"existingTexture\" returned!");
		}
		else
		{
			LOG_ERROR("Could not load texture at \"" + texturePath + "\", error: \"" + string(failResult) +
				".\" Returning solid error texture!");

			if (texture == nullptr)
			{
				texture = std::make_shared<gr::Texture>(texParams);
			}
			else
			{
				// Fill with the error texture color:
				const size_t existingWidth = texParams.m_width;
				const size_t existingHeight = texParams.m_height;
				const size_t numPixels = existingWidth * existingHeight;

				const size_t firstPixel = faceIndex * numPixels;
				const size_t lastPixel = firstPixel + numPixels - 1;
				for (size_t curPixel = faceIndex * existingWidth * existingHeight; curPixel <= lastPixel; curPixel++)
				{
					texture->Texels().at(curPixel) = texParams.m_clearColor;
				}
			}
		}

		return false;
	}


	std::shared_ptr<gr::Texture> Texture::LoadCubeMapTextureFilesFromPath(
		std::string const& textureRootPath, 
		TextureColorSpace const& colorSpace)
	{
		// Create/import cube map face textures:
		const std::string cubeTextureNames[Texture::k_numCubeFaces] =
		{
			"posx",
			"negx",
			"posy",
			"negy",
			"posz",
			"negz",
		};

		constexpr size_t NUM_FILE_EXTENSIONS = 4;
		const std::string fileExtensions[NUM_FILE_EXTENSIONS] =	// Add any desired skybox texture filetype extensions here
		{
			".jpg",
			".jpeg",
			".png",
			".tga",
		};

		std::shared_ptr<gr::Texture> cubeMapTexture(nullptr);

		for (size_t i = 0; i < Texture::k_numCubeFaces; i++)
		{
			// Search each possible file extension:
			const std::string currentCubeFaceName = textureRootPath + cubeTextureNames[i];

			// TODO: This function is horrible. If it fails for the first face (and loads the error texture), it still
			// continues to look for all other faces

			for (size_t j = 0; j < NUM_FILE_EXTENSIONS; j++)
			{
				const std::string finalName = currentCubeFaceName + fileExtensions[j];

				bool didLoad = gr::Texture::LoadTextureFileFromPath(
					cubeMapTexture, 
					finalName, 
					gr::Texture::TextureColorSpace::sRGB, 
					false,
					Texture::k_numCubeFaces,
					i);

				if (didLoad) // Stop searching
				{
					break;
				}
				else if (j == NUM_FILE_EXTENSIONS - 1)
				{
					LOG("Could not find cubemap face texture #" + to_string(i) + ": " +
						cubeTextureNames[i] + " with any supported extension. Returning null");



					return nullptr;
				}
			}
		}

		return cubeMapTexture; // Note: This still needs to be Create()'d
	}


	vec4 Texture::GetTexelDimenions() const
	{
		// .xyzw = 1/width, 1/height, width, height
		return glm::vec4(
			1.0f / m_texParams.m_width, 
			1.0f / m_texParams.m_height, 
			m_texParams.m_width, 
			m_texParams.m_height);
	}


	uint32_t Texture::GetNumMips() const
	{
		if (!m_texParams.m_useMIPs)
		{
			return 1;
		}

		const uint32_t largestDimension = glm::max(m_texParams.m_width, m_texParams.m_height);
		return (uint32_t)glm::log2((float)largestDimension) + 1;
	}

	uint32_t Texture::GetMipDimension(uint32_t mipLevel) const
	{
		// No reason we can't support non-square textures, but until we need to just assert
		SEAssert("Dimensions mismatch but (currently) assuming square texture", Width() == Height());
		return (uint32_t)(Width() / glm::pow(2.0f, mipLevel));
	}
}


