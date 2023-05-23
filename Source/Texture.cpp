// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneManager.h"
#include "Texture.h"
#include "Texture_Platform.h"

using glm::vec4;
using std::string;


namespace re
{
	std::shared_ptr<re::Texture> Texture::Create(std::string const& name, TextureParams const& params, bool doClear)
	{
		SEAssert("Invalid color space", params.m_colorSpace != re::Texture::ColorSpace::Unknown);

		// If the Texture already exists, return it. Otherwise, create the Texture 
		if (params.m_addToSceneData && en::SceneManager::GetSceneData()->TextureExists(name))
		{
			return en::SceneManager::GetSceneData()->GetTexture(name);
		}
		// Note: It's possible that 2 threads might simultaneously fail to find a Texture in the SceneData, and create
		// it. But that's OK, the SceneData will only allow 1 instance to be added

		std::shared_ptr<re::Texture> newTexture = nullptr;
		newTexture.reset(new re::Texture(name, params, doClear));

		// If requested, register the Texture with the SceneData object for lifetime management:
		bool foundExistingTexture = false;
		if (params.m_addToSceneData)
		{
			foundExistingTexture = en::SceneManager::GetSceneData()->AddUniqueTexture(newTexture);
		}
		
		// Register new Textures with the RenderManager, so their API-level objects are created before use
		if (!foundExistingTexture)
		{
			re::RenderManager::Get()->RegisterForCreate(newTexture);
		}

		return newTexture;
	}


	Texture::Texture(string const& name, TextureParams const& params, bool doClear)
		: NamedObject(name)
		, m_texParams{ params }
		, m_platformParams{ nullptr }
	{
		platform::Texture::CreatePlatformParams(*this);

		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_format);

		if (m_texParams.m_usage == Usage::Color) // Optimization: Only allocate texels for non-target types
		{
			m_texels.resize(params.m_faces * params.m_width * params.m_height * bytesPerPixel, 0);

			if (doClear) // Optimization: Only fill the texture if necessary
			{
				Fill(params.m_clearColor);
			}
		}
	}


	void Texture::Destroy()
	{
		if (m_texels.size() > 0)
		{
			m_texels.clear();
		}

		platform::Texture::Destroy(*this);

		m_platformParams = nullptr;
	}


	void Texture::SetPlatformParams(std::unique_ptr<re::Texture::PlatformParams> platformParams)
	{ 
		m_platformParams = std::move(platformParams);
	}


	std::vector<uint8_t>& Texture::GetTexels() 
	{ 
		m_platformParams->m_isDirty = true; 
		return m_texels; 
	}


	uint8_t const* Texture::GetTexel(uint32_t u, uint32_t v, uint32_t faceIdx) const
	{
		SEAssert("There are no texels. Texels are only allocated for non-target textures", m_texels.size() > 0);

		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_format);

		SEAssert("OOB texel coordinates",
			u >= 0 && 
			u < m_texParams.m_width &&
			v >= 0 && 
			v < m_texParams.m_height &&
			faceIdx < m_texParams.m_faces);

		// Number of elements in v rows, + uth element in next row
		return &m_texels[
			((faceIdx * m_texParams.m_width * m_texParams.m_height) + (v * m_texParams.m_width) + u) * bytesPerPixel];
	}


	uint8_t const* re::Texture::GetTexel(uint32_t index) const
	{
		SEAssert("There are no texels. Texels are only allocated for non-target textures", m_texels.size() > 0);

		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_format);

		SEAssert("OOB texel coordinates", 
			(index * bytesPerPixel) < (m_texParams.m_faces * m_texParams.m_width * m_texParams.m_height));

		return &m_texels[index * bytesPerPixel];
	}


	void re::Texture::SetTexel(uint32_t u, uint32_t v, glm::vec4 value)
	{
		// Note: If texture has < 4 channels, the corresponding channels in value are ignored

		SEAssert("There are no texels. Texels are only allocated for non-target textures", m_texels.size() > 0);

		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_format);

		SEAssert("OOB texel coordinates", 
			u >= 0 && 
			u  < m_texParams.m_width &&
			v >= 0 && 
			v < m_texParams.m_height);

		SEAssert("Pixel value is not normalized", 
			value.x >= 0.f && value.x <= 1.f &&
			value.y >= 0.f && value.y <= 1.f &&
			value.z >= 0.f && value.z <= 1.f &&
			value.w >= 0.f && value.w <= 1.f
		);

		// Reinterpret the value:
		void* const valuePtr = &value.x;
		void* const pixelPtr = &m_texels[((v * m_texParams.m_width) + u) * bytesPerPixel];
		switch (m_texParams.m_format)
		{
		case re::Texture::Format::RGBA32F:
		{
			*static_cast<glm::vec4*>(pixelPtr) = *static_cast<glm::vec4*>(valuePtr);
		}
		break;
		case re::Texture::Format::RGB32F:
		{
			*static_cast<glm::vec3*>(pixelPtr) = *static_cast<glm::vec3*>(valuePtr);
		}
		break;
		case re::Texture::Format::RG32F:
		{
			*static_cast<glm::vec2*>(pixelPtr) = *static_cast<glm::vec2*>(valuePtr);
		}
		break;
		case re::Texture::Format::Depth32F:
		case re::Texture::Format::R32F:
		{
			*static_cast<float*>(pixelPtr) = *static_cast<float*>(valuePtr);
		}
		break;
		case re::Texture::Format::RGBA16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 8; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
		}
		break;
		case re::Texture::Format::RGB16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 6; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
			
		}
		break;
		case re::Texture::Format::RG16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 4; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
		}
		break;
		case re::Texture::Format::R16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 2; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
		}
		break;
		case re::Texture::Format::RGBA8:
		{
			for (uint8_t i = 0; i < 4; i++)
			{
				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case re::Texture::Format::RGB8:
		{
			for (uint8_t i = 0; i < 3; i++)
			{
				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case re::Texture::Format::RG8:
		{
			for (uint8_t i = 0; i < 2; i++)
			{
				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case re::Texture::Format::R8:
		{
			const uint8_t channelValue = (uint8_t)(value[0] * 255.0f);
			*(static_cast<uint8_t*>(pixelPtr)) = channelValue;
		}
		break;
		case re::Texture::Format::Invalid:
		default:
		{
			SEAssertF("Invalid texture format to set a texel");
		}
		}

		m_platformParams->m_isDirty = true;
	}


	void re::Texture::Fill(vec4 solidColor)
	{
		SEAssert("There are no texels. Texels are only allocated for non-target textures", m_texels.size() > 0);

		for (uint32_t row = 0; row < m_texParams.m_height; row++)
		{
			for (uint32_t col = 0; col < m_texParams.m_width; col++)
			{
				SetTexel(col, row, solidColor);
			}
		}
		m_platformParams->m_isDirty = true;
	}


	void re::Texture::Fill(vec4 tl, vec4 tr, vec4 bl, vec4 br)
	{
		SEAssert("There are no texels. Texels are only allocated for non-target textures", m_texels.size() > 0);

		for (unsigned int row = 0; row < m_texParams.m_height; row++)
		{
			float vertDelta = (float)((float)row / (float)m_texParams.m_height);
			vec4 startCol = (vertDelta * bl) + ((1.0f - vertDelta) * tl);
			vec4 endCol = (vertDelta * br) + ((1.0f - vertDelta) * tr);

			for (unsigned int col = 0; col < m_texParams.m_width; col++)
			{
				float horDelta = (float)((float)col / (float)m_texParams.m_width);

				SetTexel(col, row, (horDelta * endCol) + ((1.0f - horDelta) * startCol));
			}
		}
		m_platformParams->m_isDirty = true;
	}


	vec4 Texture::GetTextureDimenions() const
	{
		// .xyzw = width, height, 1/width, 1/height
		return glm::vec4(
			m_texParams.m_width,
			m_texParams.m_height,
			1.0f / m_texParams.m_width, 
			1.0f / m_texParams.m_height);
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


	uint8_t Texture::GetNumBytesPerTexel(const Format texFormat)
	{
		switch (texFormat)
		{
		case re::Texture::Format::RGBA32F:
		{
			return 16;
		}
		break;
		case re::Texture::Format::RGB32F:
		{
			return 12;
		}
		break;
		case re::Texture::Format::RG32F:
		case re::Texture::Format::RGBA16F:
		{
			return 8;
		}
		break;
		case re::Texture::Format::RGB16F:
		{
			return 6;
		}
		break;
		case re::Texture::Format::R32F:
		case re::Texture::Format::RG16F:
		case re::Texture::Format::RGBA8:
		case re::Texture::Format::Depth32F:
		{
			return 4;
		}
		break;
		case re::Texture::Format::RGB8:
		{
			return 3;
		}
		break;
		case re::Texture::Format::R16F:
		case re::Texture::Format::RG8:
		{
			return 2;
		}
		break;
		case re::Texture::Format::R8:
		{
			return 1;
		}
		break;
		break;
		case re::Texture::Format::Invalid:
		default:
		{
			SEAssertF("Invalid texture format for stride computation");
		}
		}

		return 1;
	}

	uint8_t Texture::GetNumberOfChannels(const Format texFormat)
	{
		switch (texFormat)
		{
		case re::Texture::Format::RGBA32F:
		case re::Texture::Format::RGBA16F:
		case re::Texture::Format::RGBA8:
		{
			return 4;
		}
		break;
		case re::Texture::Format::RGB32F:
		case re::Texture::Format::RGB16F:
		case re::Texture::Format::RGB8:
		{
			return 3;
		}
		break;
		case re::Texture::Format::RG32F:
		case re::Texture::Format::RG16F:
		case re::Texture::Format::RG8:
		{
			return 2;
		}
		break;
		case re::Texture::Format::R32F:
		case re::Texture::Format::R16F:
		case re::Texture::Format::R8:
		case re::Texture::Format::Depth32F:
		{
			return 1;
		}
		case re::Texture::Format::Invalid:
		default:
		{
			SEAssertF("Invalid texture format for stride computation");
			return 1;
		}
		}
	}
}


