// © 2022 Adam Badke. All rights reserved.
#include "BindlessResource.h"
#include "RenderManager.h"
#include "Texture.h"
#include "Texture_Platform.h"

#include "Core/Assert.h"
#include "Core/Logger.h"

#include "Core/Interfaces/ILoadContext.h"

#include "Core/Util/ImGuiUtils.h"


namespace
{
	// Computes the maximum number of mip levels (e.g. 4x4 texture has 3 mip levels [0,2])
	inline uint32_t ComputeMaxMips(uint32_t width, uint32_t height)
	{
		const uint32_t largestDimension = glm::max(width, height);
		return static_cast<uint32_t>(glm::log2(static_cast<float>(std::max(width, height))) + 1);
	}


	uint32_t ComputeNumMips(re::Texture::TextureParams const& params)
	{
		if (params.m_mipMode == re::Texture::MipMode::None)
		{
			return 1;
		}

		if (params.m_numMips == re::Texture::k_allMips)
		{
			return ComputeMaxMips(params.m_width, params.m_height);
		}
		else
		{
			SEAssert(params.m_numMips > 0 &&
				params.m_numMips <= ComputeMaxMips(params.m_width, params.m_height),
					"Invalid number of mips requested");

			return params.m_numMips;
		}
	}


	uint32_t ComputeNumSubresources(re::Texture::TextureParams const& texParams)
	{
		const uint32_t numMips = ComputeNumMips(texParams);
		const uint8_t numFaces = re::Texture::GetNumFaces(texParams.m_dimension);

		uint32_t numSubresources = 0;
		if (texParams.m_dimension == re::Texture::Dimension::Texture3D)
		{
			// A 3D texture subresource is a single mipmap level, regardless of the number of depth slices etc
			// https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-textures-intro?redirectedfrom=MSDN
			numSubresources = numMips;
		}
		else
		{
			numSubresources = texParams.m_arraySize * numFaces * numMips;
		}
		return numSubresources;
	}


	inline glm::uvec2 GetMipWidthHeight(uint32_t width, uint32_t height, uint32_t mipLevel)
	{
		return glm::uvec2(
			static_cast<uint32_t>(width / static_cast<float>(glm::pow(2.0f, mipLevel))),
			static_cast<uint32_t>(height / static_cast<float>(glm::pow(2.0f, mipLevel))));
	}
}


namespace re
{
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


	uint32_t Texture::ComputeTotalBytesPerFace(re::Texture::TextureParams const& texParams, uint32_t mipLevel /*= 0*/)
	{
		glm::uvec2 const& widthHeight = GetMipWidthHeight(texParams.m_width, texParams.m_height, mipLevel);
		return widthHeight.x * widthHeight.y * re::Texture::GetNumBytesPerTexel(texParams.m_format);
	}


	void Texture::Fill(
		IInitialData* initialData, TextureParams const& texParams, glm::vec4 const& fillColor)
	{
		SEAssert(initialData->HasData(), "There are no texels. Texels are only allocated for non-target textures");

		const uint8_t numFaces = re::Texture::GetNumFaces(texParams.m_dimension);

		for (uint32_t arrayIdx = 0; arrayIdx < texParams.m_arraySize; arrayIdx++)
		{
			for (uint32_t face = 0; face < numFaces; face++)
			{
				for (uint32_t row = 0; row < texParams.m_height; row++)
				{
					for (uint32_t col = 0; col < texParams.m_width; col++)
					{
						SetTexel(initialData, texParams, arrayIdx, face, col, row, fillColor);
					}
				}
			}
		}
	}


	core::InvPtr<re::Texture> Texture::Create(
		std::string const& name,
		TextureParams const& params,
		std::vector<uint8_t>&& initialData)
	{
		struct TextureFromByteVecLoadContext final : public virtual core::ILoadContext<re::Texture>
		{
			void OnLoadBegin(core::InvPtr<re::Texture>& newTex) override
			{
				LOG(std::format("Creating texture \"{}\" from byte vector", m_texName).c_str());
				
				// Register for API-layer creation now to ensure we don't miss our chance for the current frame
				re::RenderManager::Get()->RegisterForCreate(newTex); 
			}

			std::unique_ptr<re::Texture> Load(core::InvPtr<re::Texture>& loadingTexPtr) override
			{
				const uint8_t numFaces = re::Texture::GetNumFaces(m_texParams.m_dimension);
				const uint32_t totalBytesPerFace = ComputeTotalBytesPerFace(m_texParams);

				SEAssert(m_initialDataBytes.size() == m_texParams.m_arraySize * numFaces * totalBytesPerFace,
					"Invalid data size");

				std::unique_ptr<re::Texture> tex(new re::Texture(m_texName.c_str(), m_texParams));

				tex->m_initialData = std::make_unique<re::Texture::InitialDataVec>(
					m_texParams.m_arraySize,
					numFaces,
					totalBytesPerFace,
					std::move(m_initialDataBytes));

				RegisterBindlessResourceHandles(tex.get(), loadingTexPtr);

				return tex;
			}

			std::string m_texName;
			TextureParams m_texParams;
			std::vector<uint8_t> m_initialDataBytes;
		};
		std::shared_ptr<TextureFromByteVecLoadContext> loadContext = std::make_shared<TextureFromByteVecLoadContext>();

		loadContext->m_retentionPolicy = params.m_createAsPermanent ?
			core::RetentionPolicy::Permanent :
			core::RetentionPolicy::Reusable;

		loadContext->m_texName = name;
		loadContext->m_texParams = params;
		loadContext->m_initialDataBytes = std::move(initialData);

		core::InvPtr<re::Texture> const& newTexture = re::RenderManager::Get()->GetInventory()->Get(
			util::HashKey(name),
			static_pointer_cast<core::ILoadContext<re::Texture>>(loadContext));

		return newTexture;
	}


	core::InvPtr<re::Texture> Texture::Create(std::string const& name, TextureParams const& params, glm::vec4 fillColor)
	{
		SEAssert((params.m_usage & Usage::ColorSrc), "Trying to fill a non-color texture");

		struct TextureFromColor final : public virtual core::ILoadContext<re::Texture>
		{
			void OnLoadBegin(core::InvPtr<re::Texture>& newTex) override
			{
				LOG(std::format("Creating texture \"{}\" from color", m_texName).c_str());

				// Register for API-layer creation now to ensure we don't miss our chance for the current frame
				re::RenderManager::Get()->RegisterForCreate(newTex);
			}
			
			std::unique_ptr<re::Texture> Load(core::InvPtr<re::Texture>& loadingTexPtr) override
			{
				std::unique_ptr<re::Texture::InitialDataVec> initialData = std::make_unique<re::Texture::InitialDataVec>(
					m_texParams.m_arraySize,
					re::Texture::GetNumFaces(m_texParams.m_dimension),
					re::Texture::ComputeTotalBytesPerFace(m_texParams),
					std::vector<uint8_t>());

				re::Texture::Fill(static_cast<re::Texture::IInitialData*>(initialData.get()), m_texParams, m_fillColor);

				std::unique_ptr<re::Texture> tex(new re::Texture(m_texName, m_texParams, std::move(initialData)));

				RegisterBindlessResourceHandles(tex.get(), loadingTexPtr);

				return tex;
			}

			std::string m_texName;
			re::Texture::TextureParams m_texParams;
			glm::vec4 m_fillColor;
		};
		std::shared_ptr<TextureFromColor> loadContext = std::make_shared<TextureFromColor>();

		loadContext->m_retentionPolicy = params.m_createAsPermanent ?
			core::RetentionPolicy::Permanent :
			core::RetentionPolicy::Reusable;

		loadContext->m_texName = name;
		loadContext->m_texParams = params;
		loadContext->m_fillColor = fillColor;

		return re::RenderManager::Get()->GetInventory()->Get(
			util::HashKey(name),
			static_pointer_cast<core::ILoadContext<re::Texture>>(loadContext));
	}


	core::InvPtr<re::Texture> Texture::Create(std::string const& name, TextureParams const& params)
	{
		struct RuntimeTexLoadContext final : public virtual core::ILoadContext<re::Texture>
		{
			void OnLoadBegin(core::InvPtr<re::Texture>& newTex) override
			{
				LOG(std::format("Creating runtime texture \"{}\"", m_idName).c_str());

				// Register for API-layer creation now to ensure we don't miss our chance for the current frame
				re::RenderManager::Get()->RegisterForCreate(newTex);
			}

			std::unique_ptr<re::Texture> Load(core::InvPtr<re::Texture>& loadingTexPtr) override
			{
				std::unique_ptr<re::Texture> tex(new re::Texture(m_idName, m_texParams));

				RegisterBindlessResourceHandles(tex.get(), loadingTexPtr);

				return tex;
			}

			std::string m_idName;
			re::Texture::TextureParams m_texParams;
		};
		std::shared_ptr<RuntimeTexLoadContext> loadContext = std::make_shared<RuntimeTexLoadContext>();

		loadContext->m_retentionPolicy = params.m_createAsPermanent ?
			core::RetentionPolicy::Permanent :
			core::RetentionPolicy::Reusable;

		// Runtime textures might have different parameters but use the same name (e.g. resizing an existing target
		// texture), so we append a hash of the params to the name to ensure it is unique
		const std::string runtimeName = std::format("{}_{})",
			name,
			util::HashDataBytes(&params, sizeof(re::Texture::TextureParams)));

		loadContext->m_idName = runtimeName;

		loadContext->m_texParams = params;

		return re::RenderManager::Get()->GetInventory()->Get(
			util::HashKey(runtimeName),
			static_pointer_cast<core::ILoadContext<re::Texture>>(loadContext));
	}


	void Texture::RegisterBindlessResourceHandles(re::Texture* tex, core::InvPtr<re::Texture> const& loadingTexPtr)
	{
		if (tex->HasUsageBit(re::Texture::Usage::SwapchainColorProxy) == false)
		{
			re::BindlessResourceManager* brm = re::RenderManager::Get()->GetContext()->GetBindlessResourceManager();
			if (brm)
			{
				if (tex->HasUsageBit(re::Texture::Usage::ColorSrc))
				{
					tex->m_srvResourceHandle = brm->RegisterResource(std::make_unique<re::TextureResource>(loadingTexPtr));
				}
				if (tex->HasUsageBit(re::Texture::Usage::ColorTarget))
				{
					tex->m_uavResourceHandle = brm->RegisterResource(
						std::make_unique<re::TextureResource>(loadingTexPtr, re::ViewType::UAV));
				}
			}
		}
	}


	Texture::Texture(std::string const& name, TextureParams const& params)
		: Texture(name, params, std::vector<ImageDataUniquePtr>())
	{
	}


	Texture::Texture(std::string const& name, TextureParams const& params, std::vector<ImageDataUniquePtr>&& initialData)
		: INamedObject(name)
		, m_texParams(params)
		, m_platObj(nullptr)
		, m_initialData(nullptr)
		, m_numMips(ComputeNumMips(params))
		, m_numSubresources(ComputeNumSubresources(params))
		, m_srvResourceHandle(INVALID_RESOURCE_IDX)
		, m_uavResourceHandle(INVALID_RESOURCE_IDX)
	{
		SEAssert(m_texParams.m_usage != Texture::Usage::Invalid, "Invalid usage");
		SEAssert(m_texParams.m_dimension != Texture::Dimension::Dimension_Invalid, "Invalid dimension");
		SEAssert(m_texParams.m_format != Texture::Format::Invalid, "Invalid format");
		SEAssert(m_texParams.m_colorSpace != Texture::ColorSpace::Invalid, "Invalid color space");
		SEAssert(m_texParams.m_width > 0 && m_texParams.m_height > 0, "Invalid dimensions");
		SEAssert(m_texParams.m_arraySize == 1 ||
			m_texParams.m_dimension == Dimension::Texture1DArray ||
			m_texParams.m_dimension == Dimension::Texture2DArray ||
			m_texParams.m_dimension == Dimension::Texture3D ||
			m_texParams.m_dimension == Dimension::TextureCubeArray,
			"Dimension and array size mismatch");

		SEAssert(m_texParams.m_dimension != re::Texture::Texture3D ||
			m_texParams.m_mipMode != re::Texture::MipMode::AllocateGenerate,
			"Texture3D mip generation is not (currently) supported");

		const uint8_t numFaces = re::Texture::GetNumFaces(params.m_dimension);

		if (!initialData.empty())
		{
			m_initialData = std::make_unique<InitialDataSTBIImage>(
				params.m_arraySize,
				numFaces,
				ComputeTotalBytesPerFace(params),
				std::move(initialData));
		}

		platform::Texture::CreatePlatformObject(*this);
	}


	Texture::Texture(std::string const& name, TextureParams const& params, std::unique_ptr<InitialDataVec>&& initialData)
		: INamedObject(name)
		, m_texParams(params)
		, m_platObj(nullptr)
		, m_initialData(std::move(initialData))
		, m_numMips(ComputeNumMips(params))
		, m_numSubresources(ComputeNumSubresources(params))
		, m_srvResourceHandle(INVALID_RESOURCE_IDX)
		, m_uavResourceHandle(INVALID_RESOURCE_IDX)
	{
		SEAssert(m_texParams.m_usage != Texture::Usage::Invalid, "Invalid usage");
		SEAssert(m_texParams.m_dimension != Texture::Dimension::Dimension_Invalid, "Invalid dimension");
		SEAssert(m_texParams.m_format != Texture::Format::Invalid, "Invalid format");
		SEAssert(m_texParams.m_colorSpace != Texture::ColorSpace::Invalid, "Invalid color space");
		SEAssert(m_texParams.m_width > 0 && m_texParams.m_height > 0, "Invalid dimensions");
		SEAssert(m_texParams.m_arraySize == 1 ||
			m_texParams.m_dimension == Dimension::Texture1DArray ||
			m_texParams.m_dimension == Dimension::Texture2DArray ||
			m_texParams.m_dimension == Dimension::Texture3D ||
			m_texParams.m_dimension == Dimension::TextureCubeArray,
			"Dimension and array size mismatch");

		SEAssert(m_texParams.m_dimension != re::Texture::Texture3D ||
			m_texParams.m_mipMode != re::Texture::MipMode::AllocateGenerate,
			"Texture3D mip generation is not (currently) supported");

		platform::Texture::CreatePlatformObject(*this);
	}


	Texture::~Texture()
	{
		SEAssert(m_platObj == nullptr,
			"Texture dtor called, but platform object is not null. Was Destroy() called?");
	}


	void Texture::Destroy()
	{
		LOG(std::format("Destroying texture \"{}\"", GetName()).c_str());

		platform::Texture::Destroy(*this);

		re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platObj));

		if (m_srvResourceHandle != INVALID_RESOURCE_IDX)
		{
			re::BindlessResourceManager* brm = re::RenderManager::Get()->GetContext()->GetBindlessResourceManager();
			SEAssert(brm, "Failed to get BindlessResourceManager. This should not be possible");

			brm->UnregisterResource(m_srvResourceHandle, re::RenderManager::Get()->GetCurrentRenderFrameNum());
		}

		if (m_uavResourceHandle != INVALID_RESOURCE_IDX)
		{
			re::BindlessResourceManager* brm = re::RenderManager::Get()->GetContext()->GetBindlessResourceManager();
			SEAssert(brm, "Failed to get BindlessResourceManager. This should not be possible");

			brm->UnregisterResource(m_uavResourceHandle, re::RenderManager::Get()->GetCurrentRenderFrameNum());
		}
	}


	void Texture::SetPlatformObject(std::unique_ptr<re::Texture::PlatObj> platObj)
	{ 
		m_platObj = std::move(platObj);
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
			m_initialData = nullptr;
		}
	}


	void Texture::SetTexel(
		IInitialData* initialData,
		TextureParams const& texParams,
		uint8_t arrayIdx, 
		uint32_t faceIdx,
		uint32_t u, 
		uint32_t v, 
		glm::vec4 const& value)
	{
		// Note: If texture has < 4 channels, the corresponding channels in value are ignored

		SEAssert(initialData->HasData() &&
			arrayIdx < initialData->ArrayDepth() &&
			faceIdx < initialData->NumFaces(),
			"There are no texels. Texels are only allocated for non-target textures");

		const uint8_t bytesPerPixel = GetNumBytesPerTexel(texParams.m_format);

		SEAssert(u >= 0 &&
			u < texParams.m_width &&
			v >= 0 &&
			v < texParams.m_height,
			"OOB texel coordinates");

		SEAssert(value.x >= 0.f && value.x <= 1.f &&
			value.y >= 0.f && value.y <= 1.f &&
			value.z >= 0.f && value.z <= 1.f &&
			value.w >= 0.f && value.w <= 1.f,
			"Pixel value is not normalized");

		uint8_t* dataPtr = static_cast<uint8_t*>(initialData->GetDataBytes(arrayIdx, faceIdx));
		
		// Reinterpret the value:
		void const* valuePtr = &value.x;
		void* pixelPtr = &dataPtr[((v * texParams.m_width) + u) * bytesPerPixel];
		switch (texParams.m_format)
		{
		case re::Texture::Format::RGBA32F:
		{
			*static_cast<glm::vec4*>(pixelPtr) = *static_cast<glm::vec4 const*>(valuePtr);
		}
		break;
		case re::Texture::Format::RG32F:
		{
			*static_cast<glm::vec2*>(pixelPtr) = *static_cast<glm::vec2 const*>(valuePtr);
		}
		break;
		case re::Texture::Format::R32F:
		case re::Texture::Format::Depth32F:
		{
			*static_cast<float*>(pixelPtr) = *static_cast<float const*>(valuePtr);
		}
		break;
		case re::Texture::Format::R32_UINT:
		{
			*static_cast<uint32_t*>(pixelPtr) = *static_cast<uint32_t const*>(valuePtr);
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
			*static_cast<uint16_t*>(pixelPtr) = *static_cast<uint16_t const*>(valuePtr);
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
	}


	void Texture::SetTexel(uint8_t arrayIdx, uint32_t faceIdx, uint32_t u, uint32_t v, glm::vec4 const& value)
	{
		SetTexel(m_initialData.get(), m_texParams, arrayIdx, faceIdx, u, v, value);
		m_platObj->m_isDirty = true;
	}


	void re::Texture::Fill(glm::vec4 const& solidColor)
	{
		Fill(m_initialData.get(), m_texParams, solidColor);
		m_platObj->m_isDirty = true;
	}


	glm::vec4 Texture::GetTextureDimenions() const
	{
		// .xyzw = width, height, 1/width, 1/height
		return ComputeTextureDimenions(m_texParams.m_width, m_texParams.m_height);
	}


	glm::vec4 Texture::GetMipLevelDimensions(uint32_t mipLevel) const
	{
		SEAssert(mipLevel < ComputeMaxMips(m_texParams.m_width, m_texParams.m_height), "Invalid mip level");

		glm::uvec2 const& widthHeight = GetMipWidthHeight(Width(), Height(), mipLevel);
		return glm::vec4(widthHeight.x, widthHeight.y, 1.f / widthHeight.x, 1.f / widthHeight.y);
	}


	uint32_t Texture::GetSubresourceIndex(uint32_t arrayIdx, uint32_t faceIdx, uint32_t mipIdx) const
	{
		SEAssert(mipIdx < ComputeMaxMips(m_texParams.m_width, m_texParams.m_height), "Invalid mip level");

		const uint8_t numFaces = re::Texture::GetNumFaces(this);

		SEAssert(arrayIdx < m_texParams.m_arraySize && faceIdx < numFaces && mipIdx < m_numMips, "OOB index");

		switch (m_texParams.m_dimension)
		{
		case re::Texture::Texture3D:
		{
			return mipIdx; // A Texture3D has 1 subresource per mip level
		}
		break;
		default:
			return (arrayIdx * numFaces * m_numMips) + (faceIdx * m_numMips) + mipIdx;
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


	uint8_t Texture::GetNumFaces(core::InvPtr<re::Texture> const& tex)
	{
		return GetNumFaces(tex->m_texParams.m_dimension);
	}


	uint8_t Texture::GetNumFaces(re::Texture const* tex)
	{
		return GetNumFaces(tex->m_texParams.m_dimension);
	}


	uint8_t Texture::GetNumFaces(re::Texture::Dimension dimension)
	{
		switch (dimension)
		{
		case re::Texture::TextureCube:
		case re::Texture::TextureCubeArray: return 6;
		default: return 1;
		}
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


	void Texture::ShowImGuiWindow(core::InvPtr<re::Texture> const& tex)
	{
		ImGui::Text("Texture name: \"%s\"", tex->GetName().c_str());
		ImGui::Text(std::format("Texture unique ID: {}", tex->GetUniqueID()).c_str());

		ImGui::Text(std::format("SRV resource handle: {}", tex->m_srvResourceHandle).c_str());
		ImGui::Text(std::format("UAV resource handle: {}", tex->m_uavResourceHandle).c_str());

		static size_t s_selectedIdx = 2;
		constexpr char const* k_scaleNames[] =
		{
			"10%",
			"25%",
			"50%",
			"75%",
			"100%",
		};
		util::ShowBasicComboBox(
			std::format("Texture display scale##{}", tex->GetUniqueID()).c_str(),
			k_scaleNames,
			_countof(k_scaleNames),
			s_selectedIdx);

		float scale = 1.f;
		switch (s_selectedIdx)
		{
		case 0: scale = 0.1f; break;
		case 1: scale = 0.25f; break;
		case 2: scale = 0.5f; break;
		case 3: scale = 0.75f; break;
		case 4:
		default:
			scale = 1; break;
		}

		platform::Texture::ShowImGuiWindow(tex, scale);
	}
}


