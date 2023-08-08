// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "MathUtils.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture_DX12.h"
#include "TextUtils.h"

using Microsoft::WRL::ComPtr;


namespace
{
	// Returns DXGI_FORMAT_UNKNOWN if no typeless equivalent is known
	DXGI_FORMAT GetTypelessFormatVariant(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		{
			return DXGI_FORMAT_R32G32B32A32_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
		{
			return DXGI_FORMAT_R32G32B32_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		{
			return DXGI_FORMAT_R16G16B16A16_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		{
			return DXGI_FORMAT_R32G32_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		{
			return DXGI_FORMAT_R32G8X24_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		{
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		{
			return DXGI_FORMAT_R10G10B10A2_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		{
			return DXGI_FORMAT_R8G8B8A8_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		{
			return DXGI_FORMAT_R16G16_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		{
			return DXGI_FORMAT_R32_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R24G8_TYPELESS:
		{
			return DXGI_FORMAT_R24G8_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		{
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		{
			return DXGI_FORMAT_R8G8_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		{
			return DXGI_FORMAT_R16_TYPELESS;
		}
		break;
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		{
			return DXGI_FORMAT_R8_TYPELESS;
		}
		break;
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		{
			return DXGI_FORMAT_BC1_TYPELESS;
		}
		break;
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		{
			return DXGI_FORMAT_BC2_TYPELESS;
		}
		break;
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		{
			return DXGI_FORMAT_BC3_TYPELESS;
		}
		break;
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		{
			return DXGI_FORMAT_BC4_TYPELESS;
		}
		break;
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		{
			return DXGI_FORMAT_BC5_TYPELESS;
		}
		break;
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		{
			return DXGI_FORMAT_B8G8R8A8_TYPELESS;
		}
		break;
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		{
			return DXGI_FORMAT_B8G8R8X8_TYPELESS;
		}
		break;
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		{
			return DXGI_FORMAT_BC6H_TYPELESS;
		}
		break;
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		{
			return DXGI_FORMAT_BC7_TYPELESS;
		}
		break;
		}

		return DXGI_FORMAT_UNKNOWN; // No typeless equivalent
	}


	// Returns DXGI_FORMAT_UNKNOWN if no equivalent UAV-compatible format is known
	DXGI_FORMAT GetEquivalentUAVCompatibleFormat(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case DXGI_FORMAT_R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
		case DXGI_FORMAT_R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_SINT;
		
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case DXGI_FORMAT_R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
		case DXGI_FORMAT_R16G16B16A16_UINT: return DXGI_FORMAT_R16G16B16A16_UINT;
		case DXGI_FORMAT_R16G16B16A16_SNORM: return DXGI_FORMAT_R16G16B16A16_SNORM;
		case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_SINT;

		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
		case DXGI_FORMAT_R32G32_UINT: return DXGI_FORMAT_R32G32_UINT;
		case DXGI_FORMAT_R32G32_SINT: return DXGI_FORMAT_R32G32_SINT;

		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM: return DXGI_FORMAT_R10G10B10A2_UNORM;
		case DXGI_FORMAT_R10G10B10A2_UINT: return DXGI_FORMAT_R10G10B10A2_UINT;
		case DXGI_FORMAT_R11G11B10_FLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;

		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
		
		case DXGI_FORMAT_R8G8B8A8_UINT: return DXGI_FORMAT_R8G8B8A8_UINT;
		case DXGI_FORMAT_R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
		case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_SINT;

		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
		case DXGI_FORMAT_R16G16_UNORM: return DXGI_FORMAT_R16G16_UNORM;
		case DXGI_FORMAT_R16G16_UINT: return DXGI_FORMAT_R16G16_UINT;
		case DXGI_FORMAT_R16G16_SNORM: return DXGI_FORMAT_R16G16_SNORM;
		case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_SINT;

		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_R32_UINT: return DXGI_FORMAT_R32_UINT;
		case DXGI_FORMAT_R32_SINT: return DXGI_FORMAT_R32_SINT;

		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
		case DXGI_FORMAT_R8G8_UINT: return DXGI_FORMAT_R8G8_UINT;
		case DXGI_FORMAT_R8G8_SNORM: return DXGI_FORMAT_R8G8_SNORM;
		case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_SINT;

		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT: return DXGI_FORMAT_R16_FLOAT;
		
		case DXGI_FORMAT_R16_UNORM: return DXGI_FORMAT_R16_UNORM;
		case DXGI_FORMAT_R16_UINT: return DXGI_FORMAT_R16_UINT;
		case DXGI_FORMAT_R16_SNORM: return DXGI_FORMAT_R16_SNORM;
		case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_SINT;
		
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM: return DXGI_FORMAT_R8_UNORM;
		case DXGI_FORMAT_R8_UINT: return DXGI_FORMAT_R8_UINT;
		case DXGI_FORMAT_R8_SNORM: return DXGI_FORMAT_R8_SNORM;
		case DXGI_FORMAT_R8_SINT: return DXGI_FORMAT_R8_SINT;
		case DXGI_FORMAT_A8_UNORM: return DXGI_FORMAT_A8_UNORM;
		
		case DXGI_FORMAT_B5G6R5_UNORM: return DXGI_FORMAT_B5G6R5_UNORM;
		case DXGI_FORMAT_B5G5R5A1_UNORM: return DXGI_FORMAT_B5G5R5A1_UNORM;

		case DXGI_FORMAT_B4G4R4A4_UNORM: return DXGI_FORMAT_B4G4R4A4_UNORM;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}

		return DXGI_FORMAT_UNKNOWN;
	}

	bool FormatIsUAVCompatible(DXGI_FORMAT format)
	{
		// Guaranteed UAV support: 
		if (format == DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT ||
			format == DXGI_FORMAT::DXGI_FORMAT_R32_UINT ||
			format == DXGI_FORMAT::DXGI_FORMAT_R32_SINT)
		{
			return true;
		}

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
		formatSupport.Format = format;

		HRESULT hr = device->CheckFeatureSupport(
			D3D12_FEATURE::D3D12_FEATURE_FORMAT_SUPPORT,
			&formatSupport,
			sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT));
		dx12::CheckHResult(hr, "Failed to query format support");

		return formatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
	}


	bool UAVIsNeeded(re::Texture::TextureParams const& texParams, DXGI_FORMAT dxgiFormat)
	{
		const bool compatibleUsage = 
			(texParams.m_usage & re::Texture::Usage::DepthTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::StencilTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::SwapchainColorProxy) == 0;
		if (!compatibleUsage)
		{
			return false;
		}

		const bool compatibleFormat = FormatIsUAVCompatible(dxgiFormat);
		if (!compatibleFormat)
		{
			const bool alternativeFormatExists = GetEquivalentUAVCompatibleFormat(dxgiFormat) != DXGI_FORMAT_UNKNOWN;
			if (!alternativeFormatExists)
			{
				return false;
			}
		}
		
		// We generate MIPs in DX12 via a compute shader
		const bool usesMips = texParams.m_useMIPs;
		if (!usesMips)
		{
			return false;
		}

		// TODO: We'll need to check multisampling is disabled here, once it's implemented

		return true;
	}
}

namespace dx12
{
	DXGI_FORMAT Texture::GetTextureFormat(re::Texture::TextureParams const& texParams)
	{	
		switch (texParams.m_format)
		{
			case re::Texture::Format::RGBA32F: // 32 bits per channel x N channels
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			break;
			case re::Texture::Format::RG32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;
			}
			break;
			case re::Texture::Format::R32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
			}
			break;
			case re::Texture::Format::RGBA16F: // 16 bits per channel x N channels
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
			}
			break;
			case re::Texture::Format::RG16F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT;
			}
			break;
			case re::Texture::Format::R16F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;
			}
			break;
			case re::Texture::Format::RGBA8: // 8 bits per channel x N channels
			{
				return texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ?
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			break;
			case re::Texture::Format::RG8:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM;
			}
			break;
			case re::Texture::Format::R8:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R8_UNORM;
			}
			break;
			case re::Texture::Format::Depth32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT;
			}
			break;
			case re::Texture::Format::Invalid:
			default:
			{
				SEAssertF("Invalid format");
			}
		}
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	}


	Texture::PlatformParams::PlatformParams(re::Texture::TextureParams const& texParams)
	{
		m_format = GetTextureFormat(texParams);

		for (size_t i = 0; i < dx12::Texture::View::View_Count; i++)
		{
			if (texParams.m_useMIPs)
			{
				constexpr size_t k_expectedNumMips = 10; // Assume most textures will be 1024x1024
				m_viewCpuDescAllocations[i].reserve(k_expectedNumMips);
			}
			else
			{
				m_viewCpuDescAllocations[i].reserve(1);
			}
		}
	}


	Texture::PlatformParams::~PlatformParams()
	{
		m_format = DXGI_FORMAT_UNKNOWN;
		m_textureResource = nullptr;

		for (size_t i = 0; i < dx12::Texture::View::View_Count; i++)
		{
			m_viewCpuDescAllocations[i].clear();
		}
	}


	void Texture::Create(
		re::Texture& texture,
		ID3D12GraphicsCommandList2* commandList, 
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture is already created", texPlatParams->m_isCreated == false);
		texPlatParams->m_isCreated = true;

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		// D3D12 Initial resource states:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#initial-states-for-resources
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

		uint32_t numSubresources = 0;

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		SEAssert("Invalid texture usage", texParams.m_usage > 0 && texParams.m_usage != re::Texture::Usage::Invalid);
		
		const bool needsUAV = UAVIsNeeded(texParams, texPlatParams->m_format);

		if (texParams.m_usage & re::Texture::Usage::Color)
		{
			const uint32_t numMips = texture.GetNumMips();

			// Resources can be implicitely promoted to COPY/SOURCE/COPY_DEST from COMMON, and decay to COMMON after
			// being accessed on a copy queue. So we just set the initial state as COMMON here, and not bother tracking
			// it until it's used on a non-copy queue for the first time
			initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

			numSubresources = numMips;

			re::Texture::TextureParams const& texParams = texture.GetTextureParams();

			SEAssert("TODO: Support creating per-face views for textures with muliptle faces", texParams.m_faces == 1);

			// TODO: use D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS for color textures (unless MSAA enabled)?
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_NONE;
			if (needsUAV)
			{
				flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}			

			// Resource description:
			const D3D12_RESOURCE_DESC colorResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				texPlatParams->m_format,
				texParams.m_width,
				texParams.m_height,
				texParams.m_faces,
				numMips,			// mipLevels. 0 == maximimum supported
				1,					// sampleCount TODO: Support MSAA
				0,					// sampleQuality
				flags);

			const D3D12_HEAP_PROPERTIES colorHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			HRESULT hr = device->CreateCommittedResource(
				&colorHeapProperties,
				D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
				&colorResourceDesc,
				initialState,
				nullptr, // Optimized clear value: Must be NULL except for buffers, render, or depth-stencil targets
				IID_PPV_ARGS(&texPlatParams->m_textureResource));
			CheckHResult(hr, "Failed to create color texture committed resource");

			texPlatParams->m_textureResource->SetName(texture.GetWName().c_str());

			// Assemble the subresource data:
			const uint8_t bytesPerTexel = re::Texture::GetNumBytesPerTexel(texParams.m_format);
			const uint32_t numBytes = static_cast<uint32_t>(texture.GetTexels().size());
			SEAssert("Color target must have data to buffer",
				numBytes > 0 &&
				numBytes == texParams.m_faces * texParams.m_width * texParams.m_height * bytesPerTexel);

			std::vector<D3D12_SUBRESOURCE_DATA> subresourceData;
			subresourceData.reserve(numSubresources);

			// We don't have any MIP data yet, so we only need to describe MIP 0
			subresourceData.emplace_back(D3D12_SUBRESOURCE_DATA{
				.pData = texture.GetTexels().data(),

				// https://github.com/microsoft/DirectXTex/wiki/ComputePitch
				// Row pitch: The number of bytes in a scanline of pixels: bytes-per-pixel * width-of-image
				// - Can be larger than the number of valid pixels due to alignment padding
				.RowPitch = bytesPerTexel * texParams.m_width,

				// Slice pitch: The number of bytes in each depth slice
				// - 1D/2D images: The total size of the image, including alignment padding
				.SlicePitch = numBytes
				});

			// Create an intermediate upload heap:
			// Note: If we don't request an intermediate buffer large enough, the UpdateSubresources call will return 0
			// and no update is actually recorded on the command list.
			// Buffers have the same size on all adapters: The smallest multiple of 64KB >= the buffer width
			// See remarks here:
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getresourceallocationinfo(uint_uint_constd3d12_resource_desc)
			// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 64KB, as per:
			// https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants

			const uint32_t intermediateBufferWidth =
				util::RoundUpToNearestMultiple(numBytes, static_cast<uint32_t>(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

			const D3D12_RESOURCE_DESC intermediateBufferResourceDesc =
			{
				.Dimension = D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER,
				.Alignment = 0, // 0 == default, i.e. D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 64KB
				.Width = intermediateBufferWidth,
				.Height = 1,			// Mandatory for buffers
				.DepthOrArraySize = 1,	// Mandatory for buffers
				.MipLevels = 1,			// Mandatory for buffers
				.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, // Mandatory for buffers
				.SampleDesc
				{
					.Count = 1,		// Mandatory for buffers
					.Quality = 0	// Mandatory for buffers
				},
				.Layout = D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // Mandatory for buffers
				.Flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_NONE
			};

			const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			ComPtr<ID3D12Resource> itermediateBufferResource = nullptr;

			hr = device->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
				&intermediateBufferResourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&itermediateBufferResource));
			CheckHResult(hr, "Failed to create intermediate texture buffer resource");

			const std::wstring intermediateName = texture.GetWName() + L" intermediate buffer";
			itermediateBufferResource->SetName(intermediateName.c_str());

			const uint64_t bufferSizeResult = ::UpdateSubresources(
				commandList,
				texPlatParams->m_textureResource.Get(),			// Destination resource
				itermediateBufferResource.Get(),				// Intermediate resource
				0,												// Intermediate offset
				0,												// Index of 1st subresource in the resource
				static_cast<uint32_t>(subresourceData.size()),	// Number of subresources in the subresources array
				subresourceData.data());						// Array of subresource data structs
			SEAssert("UpdateSubresources returned 0 bytes. This is unexpected", bufferSizeResult > 0);

			SEAssert("TODO: Support SRVs/UAVs for textures of different dimensions",
				texParams.m_dimension == re::Texture::Dimension::Texture2D && texParams.m_faces == 1);

			// Allocate a descriptor and create an SRV:
			{
				SEAssert("An SRV has already been created. This is unexpected", 
					texPlatParams->m_viewCpuDescAllocations[View::SRV].empty());

				texPlatParams->m_viewCpuDescAllocations[View::SRV].emplace_back(std::move(
					ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::CBV_SRV_UAV].Allocate(1)));

				dx12::DescriptorAllocation& srvDescriptorAllocation = 
					texPlatParams->m_viewCpuDescAllocations[View::SRV].back();

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
				srvDesc.Format = texPlatParams->m_format;

				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // TODO: Support different texture dimensions

				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

				srvDesc.Texture2D = D3D12_TEX2D_SRV{
					.MostDetailedMip = 0,
					.MipLevels = numMips,
					.PlaneSlice = 0,			// Index in a multi-plane format
					.ResourceMinLODClamp = 0.0f // Allow access to all MIP levels
				};

				device->CreateShaderResourceView(
					texPlatParams->m_textureResource.Get(),
					&srvDesc,
					srvDescriptorAllocation.GetBaseDescriptor());
			}

			if (needsUAV)
			{
				SEAssert("A UAV has already been created. This is unexpected",
					texPlatParams->m_viewCpuDescAllocations[View::UAV].empty());

				// We create a UAV for every MIP:
				texPlatParams->m_viewCpuDescAllocations[View::UAV].reserve(numMips);

				for (size_t mipIdx = 0; mipIdx < numMips; mipIdx++)
				{
					texPlatParams->m_viewCpuDescAllocations[View::UAV].emplace_back(std::move(
						ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::CBV_SRV_UAV].Allocate(1)));

					dx12::DescriptorAllocation const& uavDescriptorAllocation =
						texPlatParams->m_viewCpuDescAllocations[View::UAV].back();

					const DXGI_FORMAT uavCompatibleFormat = GetEquivalentUAVCompatibleFormat(texPlatParams->m_format);
					SEAssert("Failed to find equivalent UAV-compatible format", uavCompatibleFormat != DXGI_FORMAT_UNKNOWN);

					D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
						.Format = uavCompatibleFormat,
						.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
						.Texture2D = D3D12_TEX2D_UAV{
							.MipSlice = static_cast<uint32_t>(mipIdx),
							.PlaneSlice = 0}
					};

					device->CreateUnorderedAccessView(
						texPlatParams->m_textureResource.Get(),
						nullptr,		// Counter resource
						&uavDesc,
						uavDescriptorAllocation.GetBaseDescriptor());
				}
			}

			// Released once the copy is done
			intermediateResources.emplace_back(itermediateBufferResource);
		}

		if (texParams.m_usage & re::Texture::Usage::ColorTarget)
		{
			SEAssertF("TODO: Support this");
		}

		if (texParams.m_usage & re::Texture::Usage::DepthTarget)
		{
			SEAssert("Invalid usage pattern", (texParams.m_usage ^ re::Texture::Usage::DepthTarget) == 0);

			initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE;
			numSubresources = 1;

			re::Texture::TextureParams const& texParams = texture.GetTextureParams();

			const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			// Depth resource description:
			CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				texPlatParams->m_format,
				texParams.m_width,
				texParams.m_height,
				1,			// arraySize
				0,			// mipLevels
				1,			// sampleCount
				0,			// sampleQuality
				flags);

			// Depth committed heap resources:
			CD3DX12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			// Clear values:
			D3D12_CLEAR_VALUE optimizedClearValue = {};
			optimizedClearValue.Format = texPlatParams->m_format;
			optimizedClearValue.DepthStencil = { 1.0f, 0 }; // Float depth, uint8_t stencil

			HRESULT hr = device->CreateCommittedResource(
				&depthHeapProperties,
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
				&depthResourceDesc,
				initialState,
				&optimizedClearValue,
				IID_PPV_ARGS(&texPlatParams->m_textureResource));
			texPlatParams->m_textureResource->SetName(texture.GetWName().c_str());
		}

		if (texParams.m_usage & re::Texture::Usage::StencilTarget)
		{
			SEAssert("Invalid usage pattern", (texParams.m_usage ^ re::Texture::Usage::StencilTarget) == 0);

			SEAssertF("TODO: Support this");
		}

		if (texParams.m_usage & re::Texture::Usage::DepthStencilTarget)
		{
			SEAssert("Invalid usage pattern", (texParams.m_usage ^ re::Texture::Usage::DepthStencilTarget) == 0);

			SEAssertF("TODO: Support this");
		}

		if (texParams.m_usage & re::Texture::Usage::SwapchainColorProxy)
		{
			initialState = D3D12_RESOURCE_STATE_COMMON;
			numSubresources = 1;
		}


		texPlatParams->m_isDirty = true;

		// Register the resource with the global resource state tracker:
		dx12::Context::GetGlobalResourceStateTracker().RegisterResource(
			texPlatParams->m_textureResource.Get(),
			initialState,
			numSubresources);
	}


	// re::Texture::Create factory wrapper, for the DX12-specific case where we need to create a Texture resource using
	// an existing ID3D12Resource
	std::shared_ptr<re::Texture> Texture::CreateFromExistingResource(
		std::string const& name, 
		re::Texture::TextureParams const& params, 
		bool doClear, 
		Microsoft::WRL::ComPtr<ID3D12Resource> textureResource)
	{
		SEAssert("Invalid/unexpected texture format. For now, this function is used to create a backbuffer color target",
			(params.m_usage & re::Texture::Usage::SwapchainColorProxy));

		// Note: re::Texture::Create will enroll the texture in API object creation, and eventually call the standard 
		// dx12::Texture::Create above
		std::shared_ptr<re::Texture> newTexture = re::Texture::Create(name, params, doClear);

		dx12::Texture::PlatformParams* texPlatParams = 
			newTexture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture is already created", 
			!texPlatParams->m_isCreated && texPlatParams->m_textureResource == nullptr);

		texPlatParams->m_textureResource = textureResource;

		// Set the debug name:
		const std::wstring wideName = util::ToWideString(name);
		texPlatParams->m_textureResource->SetName(wideName.c_str());

		return newTexture;
	}


	void Texture::Destroy(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// Unregister the resource from the global resource state tracker. Note: The resource might be null if it was
		// never created (e.g. a duplicate was detected after loading)
		if (texPlatParams->m_textureResource)
		{
			dx12::Context::GetGlobalResourceStateTracker().UnregisterResource(texPlatParams->m_textureResource.Get());
		}

		texPlatParams->m_textureResource = nullptr;

		for (size_t i = 0; i < dx12::Texture::View::View_Count; i++)
		{
			texPlatParams->m_viewCpuDescAllocations[i].clear();
		}

		texPlatParams->m_isCreated = false;
		texPlatParams->m_isDirty = true;
	}
}