// � 2022 Adam Badke. All rights reserved.
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
	std::shared_ptr<re::Texture> Texture::Create(
		std::string const& name, 
		TextureParams const& params, 
		bool doFill, 
		glm::vec4 fillColor /*= glm::vec4(0.f, 0.f, 0.f, 1.f)*/,
		std::vector<ImageDataUniquePtr> initialData /*= std::vector<ImageDataUniquePtr>()*/)
	{
		// If the Texture already exists, return it. Otherwise, create the Texture 
		if (params.m_addToSceneData && en::SceneManager::GetSceneData()->TextureExists(name))
		{
			return en::SceneManager::GetSceneData()->GetTexture(name);
		}
		// Note: It's possible that 2 threads might simultaneously fail to find a Texture in the SceneData, and create
		// it. But that's OK, the SceneData will only allow 1 instance to be added

		std::shared_ptr<re::Texture> newTexture = nullptr;
		newTexture.reset(new re::Texture(name, params, doFill, fillColor, std::move(initialData)));

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


	Texture::Texture(
		string const& name, 
		TextureParams const& params, 
		bool doFill, 
		glm::vec4 const& fillColor, 
		std::vector<ImageDataUniquePtr> initialData /*= std::vector<ImageDataUniquePtr>()*/)
		: NamedObject(name)
		, m_texParams{ params }
		, m_platformParams{ nullptr }
		, m_initialData(std::move(initialData))
	{
		SEAssert("Invalid usage", m_texParams.m_usage != Texture::Usage::Invalid);
		SEAssert("Invalid dimension", m_texParams.m_dimension != Texture::Dimension::Dimension_Invalid);
		SEAssert("Invalid format", m_texParams.m_format != Texture::Format::Invalid);
		SEAssert("Invalid color space", m_texParams.m_colorSpace != Texture::ColorSpace::Invalid);
		SEAssert("Invalid dimensions", m_texParams.m_width > 0 && m_texParams.m_height > 0);
		SEAssert("Cubemap textures must have exactly 6 faces", 
			m_texParams.m_dimension != Texture::Dimension::TextureCubeMap || m_texParams.m_faces == 6);

		platform::Texture::CreatePlatformParams(*this);

		if (m_texParams.m_usage & Usage::Color) // Optimization: Only allocate texels for non-target types
		{
			if (m_initialData.empty())
			{
				const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_format);
				const uint32_t totalBytesPerFace = params.m_width * params.m_height * bytesPerPixel;

				m_initialData.resize(params.m_faces);

				for (size_t faceIdx = 0; faceIdx < params.m_faces; faceIdx++)
				{
					m_initialData[faceIdx] = std::move(re::Texture::ImageDataUniquePtr(
						new uint8_t[totalBytesPerFace],
						[](void* imageData) { delete[] imageData; }));
				}
			}

			if (doFill) // Optimization: Only fill the texture if necessary
			{
				Fill(fillColor);
			}
		}
	}


	Texture::~Texture()
	{
		m_initialData.clear();

		platform::Texture::Destroy(*this);

		m_platformParams = nullptr;
	}


	void Texture::SetPlatformParams(std::unique_ptr<re::Texture::PlatformParams> platformParams)
	{ 
		m_platformParams = std::move(platformParams);
	}


	size_t Texture::GetTotalBytesPerFace() const
	{
		return m_texParams.m_width * m_texParams.m_height * GetNumBytesPerTexel(m_texParams.m_format);
	}


	void* Texture::GetTexelData(uint8_t faceIdx) const
	{
		return m_initialData[faceIdx].get();
	}


	void re::Texture::SetTexel(uint32_t face, uint32_t u, uint32_t v, glm::vec4 value)
	{
		// Note: If texture has < 4 channels, the corresponding channels in value are ignored

		SEAssert("There are no texels. Texels are only allocated for non-target textures", 
			!m_initialData.empty() && face < m_initialData.size());

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

		uint8_t* dataPtr = static_cast<uint8_t*>(GetTexelData(face));

		// Reinterpret the value:
		void* const valuePtr = &value.x;
		void* const pixelPtr = &dataPtr[((v * m_texParams.m_width) + u) * bytesPerPixel];
		switch (m_texParams.m_format)
		{
		case re::Texture::Format::RGBA32F:
		{
			*static_cast<glm::vec4*>(pixelPtr) = *static_cast<glm::vec4*>(valuePtr);
		}
		break;
		case re::Texture::Format::RG32F:
		{
			*static_cast<glm::vec2*>(pixelPtr) = *static_cast<glm::vec2*>(valuePtr);
		}
		break;
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
				SEAssert("Expected a normalized float", value[i] >= 0 && value[i] <= 1.f);

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
		case re::Texture::Format::Depth32F:
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
		SEAssert("There are no texels. Texels are only allocated for non-target textures", !m_initialData.empty());

		for (uint32_t face = 0; face < m_texParams.m_faces; face++)
		{
			for (uint32_t row = 0; row < m_texParams.m_height; row++)
			{
				for (uint32_t col = 0; col < m_texParams.m_width; col++)
				{
					SetTexel(face, col, row, solidColor);
				}
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
		if (m_texParams.m_mipMode == re::Texture::MipMode::None)
		{
			return 1;
		}

		const uint32_t largestDimension = glm::max(m_texParams.m_width, m_texParams.m_height);
		return (uint32_t)glm::log2((float)largestDimension) + 1;
	}


	uint32_t Texture::GetTotalNumSubresources() const
	{
		return GetNumMips() * m_texParams.m_faces;
	}


	glm::vec4 Texture::GetSubresourceDimensions(uint32_t mipLevel) const
	{
		const float widthDims = Width() / static_cast<float>(glm::pow(2.0f, mipLevel));
		const float heightDims = Height() / static_cast<float>(glm::pow(2.0f, mipLevel));
		return glm::vec4(widthDims, heightDims, 1.f / widthDims, 1.f / heightDims);
	}


	bool Texture::IsPowerOfTwo() const
	{
		const uint32_t width = Width();
		const uint32_t height = Height();
		SEAssert("Invalid texture dimensions", width > 0 && height > 0);

		// A power-of-two value will only have a single bit set to 1 in its binary representation; Use a logical AND
		// to check if this is the case for both texture dimensions
		return ((width & (width - 1)) == 0) && ((height & (height - 1)) == 0);
	}


	bool Texture::IsSRGB() const
	{
		return m_texParams.m_colorSpace == ColorSpace::sRGB;
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
		case re::Texture::Format::RG32F:
		case re::Texture::Format::RGBA16F:
		{
			return 8;
		}
		break;
		case re::Texture::Format::R32F:
		case re::Texture::Format::RG16F:
		case re::Texture::Format::RGBA8:
		{
			return 4;
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
		case re::Texture::Format::Depth32F:
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
		{
			return 1;
		}
		case re::Texture::Format::Depth32F:
		case re::Texture::Format::Invalid:
		default:
		{
			SEAssertF("Invalid texture format for stride computation");
			return 1;
		}
		}
	}
}


