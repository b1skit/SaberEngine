#include "Texture.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"

using glm::vec4;
using std::string;


namespace gr
{
	Texture::Texture(string const& name, TextureParams const& params) :
			NamedObject(name),
		m_texParams{ params },
		m_isCreated{ false },
		m_isDirty{ true },
		m_platformParams{ nullptr } // Initialized during Create(), to ensure the texture is correctly configured
	{
		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_texFormat);
		
		m_texels.resize(params.m_faces * params.m_width * params.m_height * bytesPerPixel);
		Fill(params.m_clearColor);
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


	uint8_t const* gr::Texture::GetTexel(uint32_t u, uint32_t v, uint32_t faceIdx) const
	{
		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_texFormat);

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


	uint8_t const* gr::Texture::GetTexel(uint32_t index) const
	{
		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_texFormat);

		SEAssert("OOB texel coordinates", 
			(index * bytesPerPixel) < (m_texParams.m_faces * m_texParams.m_width * m_texParams.m_height));

		return &m_texels[index * bytesPerPixel];
	}


	void gr::Texture::SetTexel(uint32_t u, uint32_t v, glm::vec4 value)
	{
		// Note: If texture has < 4 channels, the corresponding channels in value are ignored

		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_texFormat);

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
		switch (m_texParams.m_texFormat)
		{
		case gr::Texture::TextureFormat::RGBA32F:
		{
			*static_cast<glm::vec4*>(pixelPtr) = *static_cast<glm::vec4*>(valuePtr);
		}
		break;
		case gr::Texture::TextureFormat::RGB32F:
		{
			*static_cast<glm::vec3*>(pixelPtr) = *static_cast<glm::vec3*>(valuePtr);
		}
		break;
		case gr::Texture::TextureFormat::RG32F:
		{
			*static_cast<glm::vec2*>(pixelPtr) = *static_cast<glm::vec2*>(valuePtr);
		}
		break;
		case gr::Texture::TextureFormat::Depth32F:
		case gr::Texture::TextureFormat::R32F:
		{
			*static_cast<float*>(pixelPtr) = *static_cast<float*>(valuePtr);
		}
		break;
		case gr::Texture::TextureFormat::RGBA16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 8; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
		}
		break;
		case gr::Texture::TextureFormat::RGB16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 6; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
			
		}
		break;
		case gr::Texture::TextureFormat::RG16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 4; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
		}
		break;
		case gr::Texture::TextureFormat::R16F:
		{
			// TODO: Support half-precision floats. For now, just fill with black
			for (size_t numBytes = 0; numBytes < 2; numBytes++)
			{
				*(static_cast<uint8_t*>(pixelPtr) + numBytes) = 0;
			}
		}
		break;
		case gr::Texture::TextureFormat::RGBA8:
		{
			for (uint8_t i = 0; i < 4; i++)
			{
				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case gr::Texture::TextureFormat::RGB8:
		{
			for (uint8_t i = 0; i < 3; i++)
			{
				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case gr::Texture::TextureFormat::RG8:
		{
			for (uint8_t i = 0; i < 2; i++)
			{
				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case gr::Texture::TextureFormat::R8:
		{
			const uint8_t channelValue = (uint8_t)(value[0] * 255.0f);
			*(static_cast<uint8_t*>(pixelPtr)) = channelValue;
		}
		break;
		case gr::Texture::TextureFormat::Invalid:
		default:
		{
			SEAssert("Invalid texture format to set a texel", false);
		}
		}

		m_isDirty = true;
	}


	void gr::Texture::Fill(vec4 solidColor)
	{
		for (uint32_t row = 0; row < m_texParams.m_height; row++)
		{
			for (uint32_t col = 0; col < m_texParams.m_width; col++)
			{
				SetTexel(col, row, solidColor);
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

				SetTexel(col, row, (horDelta * endCol) + ((1.0f - horDelta) * startCol));
			}
		}
		m_isDirty = true;
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


	uint8_t Texture::GetNumBytesPerTexel(const TextureFormat texFormat)
	{
		switch (texFormat)
		{
		case gr::Texture::TextureFormat::RGBA32F:
		{
			return 16;
		}
		break;
		case gr::Texture::TextureFormat::RGB32F:
		{
			return 12;
		}
		break;
		case gr::Texture::TextureFormat::RG32F:
		case gr::Texture::TextureFormat::RGBA16F:
		{
			return 8;
		}
		break;
		case gr::Texture::TextureFormat::RGB16F:
		{
			return 6;
		}
		break;
		case gr::Texture::TextureFormat::R32F:
		case gr::Texture::TextureFormat::RG16F:
		case gr::Texture::TextureFormat::RGBA8:
		case gr::Texture::TextureFormat::Depth32F:
		{
			return 4;
		}
		break;
		case gr::Texture::TextureFormat::RGB8:
		{
			return 3;
		}
		break;
		case gr::Texture::TextureFormat::R16F:
		case gr::Texture::TextureFormat::RG8:
		{
			return 2;
		}
		break;
		case gr::Texture::TextureFormat::R8:
		{
			return 1;
		}
		break;
		break;
		case gr::Texture::TextureFormat::Invalid:
		default:
		{
			SEAssert("Invalid texture format for stride computation", false);
		}
		}

		return 1;
	}

	uint8_t Texture::GetNumberOfChannels(const TextureFormat texFormat)
	{
		switch (texFormat)
		{
		case gr::Texture::TextureFormat::RGBA32F:
		case gr::Texture::TextureFormat::RGBA16F:
		case gr::Texture::TextureFormat::RGBA8:
		{
			return 4;
		}
		break;
		case gr::Texture::TextureFormat::RGB32F:
		case gr::Texture::TextureFormat::RGB16F:
		case gr::Texture::TextureFormat::RGB8:
		{
			return 3;
		}
		break;
		case gr::Texture::TextureFormat::RG32F:
		case gr::Texture::TextureFormat::RG16F:
		case gr::Texture::TextureFormat::RG8:
		{
			return 2;
		}
		break;
		case gr::Texture::TextureFormat::R32F:
		case gr::Texture::TextureFormat::R16F:
		case gr::Texture::TextureFormat::R8:
		case gr::Texture::TextureFormat::Depth32F:
		{
			return 1;
		}
		case gr::Texture::TextureFormat::Invalid:
		default:
		{
			SEAssert("Invalid texture format for stride computation", false);
			return 1;
		}
		}
	}
}


