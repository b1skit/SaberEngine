// © 2022 Adam Badke. All rights reserved.
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneManager.h"
#include "Texture.h"
#include "Texture_Platform.h"

#include "Core/Assert.h"

#include "Core/Util/CastUtils.h"


namespace
{
	uint32_t ComputeNumMips(re::Texture::TextureParams const& params)
	{
		if (params.m_mipMode == re::Texture::MipMode::None)
		{
			return 1;
		}

		return re::Texture::ComputeMaxMips(params.m_width, params.m_height);
	}


	uint32_t ComputeNumSubresources(re::Texture::TextureParams const& texParams)
	{
		const uint32_t numMips = ComputeNumMips(texParams);

		uint32_t numSubresources = 0;
		if (texParams.m_dimension == re::Texture::Dimension::Texture3D)
		{
			// A 3D texture subresource is a single mipmap level, regardless of the number of depth slices etc
			// https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-textures-intro?redirectedfrom=MSDN
			numSubresources = numMips;
		}
		else
		{
			numSubresources = texParams.m_arraySize * texParams.m_faces * numMips;
		}
		return numSubresources;
	}


	glm::uvec2 GetMipWidthHeight(uint32_t width, uint32_t height, uint32_t mipLevel)
	{
		return glm::uvec2(
			static_cast<uint32_t>(width / static_cast<float>(glm::pow(2.0f, mipLevel))),
			static_cast<uint32_t>(height / static_cast<float>(glm::pow(2.0f, mipLevel))));
	}


	uint32_t ComputeTotalBytesPerFace(re::Texture::TextureParams const& texParams, uint32_t mipLevel = 0)
	{
		glm::uvec2 const& widthHeight = GetMipWidthHeight(texParams.m_width, texParams.m_height, mipLevel);
		return widthHeight.x * widthHeight.y * re::Texture::GetNumBytesPerTexel(texParams.m_format);
	}
}


namespace re
{
	uint32_t Texture::ComputeMaxMips(uint32_t width, uint32_t height)
	{
		const uint32_t largestDimension = glm::max(width, height);
		return (uint32_t)glm::log2((float)largestDimension) + 1;
	}


	glm::vec4 Texture::ComputeTextureDimenions(uint32_t width, uint32_t height)
	{
		// .xyzw = width, height, 1/width, 1/height
		return glm::vec4(
			width,
			height,
			1.f / width,
			1.f / height);
	}


	glm::vec4 Texture::ComputeTextureDimenions(glm::uvec2 widthHeight)
	{
		return ComputeTextureDimenions(widthHeight.x, widthHeight.y);
	}


	std::shared_ptr<re::Texture> Texture::Create(
		std::string const& name, 
		TextureParams const& params, 
		std::vector<ImageDataUniquePtr>&& initialData)
	{
		SEAssert(initialData.size() == params.m_faces, "Number of initial data entries must match the number of faces");
		
		SEAssert(params.m_dimension == Texture1D || 
			params.m_dimension == Texture1DArray ||
			params.m_dimension == Texture2D ||
			params.m_dimension == Texture2DArray ||
			params.m_dimension == TextureCubeMap ||
			params.m_dimension == TextureCubeMapArray,
			"Only Textures with initial data that can be non-contiguous in memory at buffer time are supported here");

		std::unique_ptr<IInitialData> initialDataPtr = std::make_unique<InitialDataSTBIImage>(
			params.m_arraySize,
			params.m_faces,
			ComputeTotalBytesPerFace(params),
			std::move(initialData));

		std::shared_ptr<re::Texture> newTex = CreateInternal(name, params, std::move(initialDataPtr));

		return newTex;
	}


	std::shared_ptr<re::Texture> Texture::Create(
		std::string const& name,
		TextureParams const& params,
		std::vector<uint8_t>&& initialData)
	{
		SEAssert(initialData.size() == params.m_arraySize * params.m_faces * ComputeTotalBytesPerFace(params),
			"Invalid data size");

		std::unique_ptr<IInitialData> initialDataPtr = std::make_unique<InitialDataVec>(
			params.m_arraySize,
			params.m_faces,
			ComputeTotalBytesPerFace(params),
			std::move(initialData));

		std::shared_ptr<re::Texture> newTex = CreateInternal(name, params, std::move(initialDataPtr));

		return newTex;
	}


	std::shared_ptr<re::Texture> Texture::Create(
		std::string const& name,
		TextureParams const& params,
		glm::vec4 fillColor)
	{
		SEAssert((params.m_usage & Usage::Color), "Trying to fill a non-color texture");

		std::unique_ptr<IInitialData> initialData = std::make_unique<InitialDataVec>(
			params.m_arraySize,
			params.m_faces,
			ComputeTotalBytesPerFace(params),
			std::vector<uint8_t>());

		std::shared_ptr<re::Texture> newTex = CreateInternal(name, params, std::move(initialData));

		newTex->Fill(fillColor);
		
		return newTex;
	}


	std::shared_ptr<re::Texture> Texture::Create(std::string const& name, TextureParams const& params)
	{
		SEAssert((params.m_usage ^ Usage::Color), "Textures with Usage::Color only must have initial data");
		return CreateInternal(name, params, nullptr);
	}


	std::shared_ptr<re::Texture> Texture::CreateInternal(
		std::string const& name, TextureParams const& params, std::unique_ptr<IInitialData>&& initialData)
	{
		std::shared_ptr<re::Texture> newTexture = nullptr;
		{
			static std::mutex s_createInternalMutex;
			std::lock_guard<std::mutex> lock(s_createInternalMutex);

			// If the Texture already exists, return it. Otherwise, create the Texture 
			if (params.m_addToSceneData && fr::SceneManager::GetSceneData()->TextureExists(name))
			{
				// Note: In this case, we're assuming the texture is identical and ignoring the initial data
				return fr::SceneManager::GetSceneData()->GetTexture(name);
			}


			newTexture.reset(new re::Texture(name, params));

			// If requested, register the Texture with the SceneData object for lifetime management:
			bool foundExistingTexture = false;
			if (params.m_addToSceneData)
			{
				foundExistingTexture = fr::SceneManager::GetSceneData()->AddUniqueTexture(newTexture);
			}
			SEAssert(!foundExistingTexture, "Found an existing texture, this should not be possible due to the local mutex");

			if (initialData)
			{
				newTexture->m_initialData = std::move(initialData);
			}

			// Register new Textures with the RenderManager, so their API-level objects are created before use
			re::RenderManager::Get()->RegisterForCreate(newTexture);
		}

		return newTexture;
	}


	Texture::Texture(
		std::string const& name,
		TextureParams const& params)
		: INamedObject(name)
		, m_texParams(params)
		, m_platformParams(nullptr)
		, m_initialData(nullptr)
		, m_numMips(ComputeNumMips(params))
		, m_numSubresources(ComputeNumSubresources(params))
	{
		SEAssert(m_texParams.m_usage != Texture::Usage::Invalid, "Invalid usage");
		SEAssert(m_texParams.m_dimension != Texture::Dimension::Dimension_Invalid, "Invalid dimension");
		SEAssert(m_texParams.m_format != Texture::Format::Invalid, "Invalid format");
		SEAssert(m_texParams.m_colorSpace != Texture::ColorSpace::Invalid, "Invalid color space");
		SEAssert(m_texParams.m_width > 0 && m_texParams.m_height > 0, "Invalid dimensions");
		SEAssert((m_texParams.m_faces == 1 && 
			(m_texParams.m_dimension != Texture::Dimension::TextureCubeMap ||
				m_texParams.m_dimension != Texture::Dimension::TextureCubeMapArray)) ||
			(m_texParams.m_faces == 6 && 
				(m_texParams.m_dimension == Texture::Dimension::TextureCubeMap ||
					m_texParams.m_dimension == Texture::Dimension::TextureCubeMapArray)),
			"Dimension and number of faces mismatch");
		SEAssert(m_texParams.m_arraySize == 1 || 
			m_texParams.m_dimension == Dimension::Texture1DArray ||
			m_texParams.m_dimension == Dimension::Texture2DArray ||
			m_texParams.m_dimension == Dimension::Texture3D ||
			m_texParams.m_dimension == Dimension::TextureCubeMapArray,
			"Dimension and array size mismatch");

		SEAssert(m_texParams.m_dimension != re::Texture::Texture3D || 
			m_texParams.m_mipMode != re::Texture::MipMode::AllocateGenerate,
			"Texture3D mip generation is not (currently) supported");

		platform::Texture::CreatePlatformParams(*this);
	}


	Texture::~Texture()
	{
		m_initialData = nullptr;

		platform::Texture::Destroy(*this);

		m_platformParams = nullptr;
	}


	void Texture::SetPlatformParams(std::unique_ptr<re::Texture::PlatformParams> platformParams)
	{ 
		m_platformParams = std::move(platformParams);
	}


	uint32_t Texture::GetTotalBytesPerFace(uint32_t mipLevel /*= 0*/) const
	{
		return ComputeTotalBytesPerFace(m_texParams);
	}


	bool Texture::HasInitialData() const
	{
		return m_initialData && m_initialData->HasData();
	}


	void* Texture::GetTexelData(uint8_t arrayIdx, uint8_t faceIdx) const
	{
		if (!m_initialData->HasData())
		{
			return nullptr;
		}
		return m_initialData->GetDataBytes(arrayIdx, faceIdx);
	}


	void Texture::ClearTexelData()
	{
		if (m_initialData)
		{
			m_initialData->Clear();
		}
		m_initialData = nullptr;
	}


	void Texture::SetTexel(uint8_t arrayIdx, uint32_t faceIdx, uint32_t u, uint32_t v, glm::vec4 value)
	{
		// Note: If texture has < 4 channels, the corresponding channels in value are ignored

		SEAssert(m_initialData->HasData() && 
			arrayIdx < m_initialData->ArrayDepth() && 
			faceIdx < m_initialData->NumFaces(),
			"There are no texels. Texels are only allocated for non-target textures");

		const uint8_t bytesPerPixel = GetNumBytesPerTexel(m_texParams.m_format);

		SEAssert(u >= 0 && 
			u  < m_texParams.m_width &&
			v >= 0 && 
			v < m_texParams.m_height, 
			"OOB texel coordinates");

		SEAssert(value.x >= 0.f && value.x <= 1.f &&
			value.y >= 0.f && value.y <= 1.f &&
			value.z >= 0.f && value.z <= 1.f &&
			value.w >= 0.f && value.w <= 1.f,
			"Pixel value is not normalized");

		uint8_t* dataPtr = static_cast<uint8_t*>(GetTexelData(arrayIdx, faceIdx));

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
		case re::Texture::Format::Depth32F:
		{
			*static_cast<float*>(pixelPtr) = *static_cast<float*>(valuePtr);
		}
		break;
		case re::Texture::Format::R32_UINT:
		{
			*static_cast<uint32_t*>(pixelPtr) = *static_cast<uint32_t*>(valuePtr);
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
		case re::Texture::Format::R16_UNORM:
		{
			*static_cast<uint16_t*>(pixelPtr) = *static_cast<uint16_t*>(valuePtr);
		}
		break;
		case re::Texture::Format::RGBA8_UNORM:
		{
			for (uint8_t i = 0; i < 4; i++)
			{
				SEAssert(value[i] >= 0 && value[i] <= 1.f, "Expected a normalized float");

				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case re::Texture::Format::RG8_UNORM:
		{
			for (uint8_t i = 0; i < 2; i++)
			{
				const uint8_t channelValue = (uint8_t)(value[i] * 255.0f);
				*(static_cast<uint8_t*>(pixelPtr) + i) = channelValue;
			}
		}
		break;
		case re::Texture::Format::R8_UNORM:
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


	void re::Texture::Fill(glm::vec4 solidColor)
	{
		SEAssert(m_initialData->HasData(), "There are no texels. Texels are only allocated for non-target textures");

		for (uint32_t arrayIdx = 0; arrayIdx < m_texParams.m_arraySize; arrayIdx++)
		{
			for (uint32_t face = 0; face < m_texParams.m_faces; face++)
			{
				for (uint32_t row = 0; row < m_texParams.m_height; row++)
				{
					for (uint32_t col = 0; col < m_texParams.m_width; col++)
					{
						SetTexel(arrayIdx, face, col, row, solidColor);
					}
				}
			}
		}
		m_platformParams->m_isDirty = true;
	}


	glm::vec4 Texture::GetTextureDimenions() const
	{
		// .xyzw = width, height, 1/width, 1/height
		return ComputeTextureDimenions(m_texParams.m_width, m_texParams.m_height);
	}


	glm::vec4 Texture::GetMipLevelDimensions(uint32_t mipLevel) const
	{
		glm::uvec2 const& widthHeight = GetMipWidthHeight(Width(), Height(), mipLevel);
		return glm::vec4(widthHeight.x, widthHeight.y, 1.f / widthHeight.x, 1.f / widthHeight.y);
	}


	uint32_t Texture::GetSubresourceIndex(uint32_t arrayIdx, uint32_t faceIdx, uint32_t mipIdx) const
	{
		SEAssert(arrayIdx < m_texParams.m_arraySize && faceIdx < m_texParams.m_faces && mipIdx < m_numMips, "OOB index");

		switch (m_texParams.m_dimension)
		{
		case re::Texture::Texture3D:
		{
			return mipIdx; // A Texture3D has 1 subresource per mip level
		}
		break;
		default:
			return (arrayIdx * m_texParams.m_faces * m_numMips) + (faceIdx * m_numMips) + mipIdx;
		}
	}


	bool Texture::IsPowerOfTwo() const
	{
		const uint32_t width = Width();
		const uint32_t height = Height();
		SEAssert(width > 0 && height > 0, "Invalid texture dimensions");

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
		case re::Texture::Format::R32_UINT:
		case re::Texture::Format::Depth32F:
		case re::Texture::Format::RG16F:
		case re::Texture::Format::RGBA8_UNORM:
		{
			return 4;
		}
		break;
		case re::Texture::Format::R16F:
		case re::Texture::Format::R16_UNORM:
		case re::Texture::Format::RG8_UNORM:
		{
			return 2;
		}
		break;
		case re::Texture::Format::R8_UNORM:
		{
			return 1;
		}
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
		case re::Texture::Format::RGBA8_UNORM:
		{
			return 4;
		}
		break;
		case re::Texture::Format::RG32F:
		case re::Texture::Format::RG16F:
		case re::Texture::Format::RG8_UNORM:
		{
			return 2;
		}
		break;
		case re::Texture::Format::R32F:
		case re::Texture::Format::R32_UINT:
		case re::Texture::Format::R16F:
		case re::Texture::Format::R16_UNORM:
		case re::Texture::Format::R8_UNORM:
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


	// ---


	Texture::InitialDataSTBIImage::InitialDataSTBIImage(
		uint32_t arrayDepth, uint8_t numFaces, uint32_t bytesPerFace, std::vector<ImageDataUniquePtr>&& initialData)
		: IInitialData(arrayDepth, numFaces, bytesPerFace)
	{
		SEAssert(arrayDepth * numFaces == initialData.size(),
			"Array depth and number of faces don't match the number of elements in the inital data vector");

		SEAssert(!initialData.empty(), "Initial data is empty. This is unexpected for STBI image data");

		m_data = std::move(initialData);
	}


	bool Texture::InitialDataSTBIImage::HasData() const 
	{
		return !m_data.empty();
	}


	void* Texture::InitialDataSTBIImage::GetDataBytes(uint8_t arrayIdx, uint8_t faceIdx)
	{
		const size_t dataIdx = (static_cast<size_t>(arrayIdx) * m_numFaces) + faceIdx;

		SEAssert(arrayIdx < m_arrayDepth && 
			faceIdx < m_numFaces &&
			dataIdx < m_data.size(),
			"Face index OOB");

		return m_data[dataIdx].get();
	}
	

	void Texture::InitialDataSTBIImage::Clear()
	{
		m_data.clear();
	}


	// ---


	Texture::InitialDataVec::InitialDataVec(
		uint32_t arrayDepth, uint8_t numFaces, uint32_t bytesPerFace, std::vector<uint8_t>&& initialData)
		: IInitialData(arrayDepth, numFaces, bytesPerFace)
	{
		SEAssert((initialData.size() % (static_cast<uint32_t>(numFaces) * bytesPerFace)) == 0,
			"Received parameters and data size mismatch");

		m_data = std::move(initialData);

		if (m_data.empty())
		{
			const uint32_t totalBytes = m_arrayDepth * m_numFaces * m_bytesPerFace;
			m_data.resize(totalBytes);
		}
	}


	bool Texture::InitialDataVec::HasData() const
	{
		return !m_data.empty();
	}


	void* Texture::InitialDataVec::GetDataBytes(uint8_t arrayIdx, uint8_t faceIdx)
	{
		SEAssert(arrayIdx < m_arrayDepth && faceIdx < m_numFaces, "An index is OOB");

		return &m_data[(arrayIdx * m_numFaces * m_bytesPerFace) + (faceIdx * m_bytesPerFace)];
	}


	void Texture::InitialDataVec::Clear()
	{
		m_data.clear();
	}


	// ---


	void Texture::ShowImGuiWindow() const
	{
		ImGui::Text("Texture name: \"%s\"", GetName().c_str());
		ImGui::Text(std::format("Texture unique ID: {}", GetUniqueID()).c_str());
	}
}


