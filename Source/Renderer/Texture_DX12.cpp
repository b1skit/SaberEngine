// © 2022 Adam Badke. All rights reserved.
#include "CommandList_DX12.h"
#include "Context_DX12.h"
#include "HeapManager_DX12.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/MathUtils.h"
#include "Core/Util/TextUtils.h"

#include <d3dx12.h>

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


	bool FormatIsUAVCompatible(DXGI_FORMAT format)
	{
		// Guaranteed UAV support: 
		if (format == DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT ||
			format == DXGI_FORMAT::DXGI_FORMAT_R32_UINT ||
			format == DXGI_FORMAT::DXGI_FORMAT_R32_SINT)
		{
			return true;
		}

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
		formatSupport.Format = format;

		HRESULT hr = device->CheckFeatureSupport(
			D3D12_FEATURE::D3D12_FEATURE_FORMAT_SUPPORT,
			&formatSupport,
			sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT));
		dx12::CheckHResult(hr, "Failed to query format support");

		return formatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
	}


	bool SimultaneousAccessIsNeeded(re::Texture::TextureParams const& texParams)
	{
		// Assume that if a resource is used as a target and anything else, it could be used simultaneously
		const bool usedAsMoreThanTarget = 
			((texParams.m_usage & re::Texture::Usage::ColorTarget) &&
			(texParams.m_usage ^ re::Texture::Usage::ColorTarget));
		if (!usedAsMoreThanTarget)
		{
			return false;
		}

		// As per the documentation, simultaneous access cannot be used with buffers, MSAA textures, or when the
		// D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL flag is used
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags

		const bool doesNotUseMSAA = (texParams.m_multisampleMode == re::Texture::MultisampleMode::Disabled);
		if (!doesNotUseMSAA)
		{
			return false;
		}

		const bool isNotDepthStencil = !(texParams.m_usage & re::Texture::Usage::DepthTarget) && 
			!(texParams.m_usage & re::Texture::Usage::StencilTarget) &&
			!(texParams.m_usage & re::Texture::Usage::DepthStencilTarget);
		if (!isNotDepthStencil)
		{
			return false;
		}

		const bool isNotSwapchain = !(texParams.m_usage & re::Texture::Usage::SwapchainColorProxy);
		if (!isNotSwapchain)
		{
			return false;
		}

		return true;
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
			const bool alternativeFormatExists = 
				dx12::Texture::GetEquivalentUAVCompatibleFormat(dxgiFormat) != DXGI_FORMAT_UNKNOWN;
			if (!alternativeFormatExists)
			{
				return false;
			}
		}

		// By now, we know a UAV is possible. Return true for any case where it's actually needed
		
		const bool isTarget = (texParams.m_usage & re::Texture::Usage::ColorTarget) != 0;
		if (isTarget)
		{
			return true;
		}

		// We generate MIPs in DX12 via a compute shader
		if (texParams.m_mipMode == re::Texture::MipMode::AllocateGenerate)
		{
			return true;
		}

		// We didn't hit a case where a UAV is explicitely needed
		return false;
	}


	// Returns the initial state
	D3D12_RESOURCE_STATES CreateTextureResource(re::Texture& texture, bool needsUAV, bool simultaneousAccess)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert(!texPlatParams->m_gpuResource, "Texture resource already created");
		
		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		// We'll update these settings for each type of texture resource:
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		if (needsUAV)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		if (simultaneousAccess)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
		}

		// Resources can be implicitely promoted to COPY/SOURCE/COPY_DEST from COMMON, and decay to COMMON after
		// being accessed on a copy queue. For now, we (typically) set the initial state as COMMON for everything until
		// more complex cases arise
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		// Note: optimizedClearValuePtr is ignored unless:
		// - D3D12_RESOURCE_DESC::Dimension is D3D12_RESOURCE_DIMENSION_BUFFER,
		// - D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET or D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL are set in flags
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = texPlatParams->m_format;
		
		if (texParams.m_usage & re::Texture::Usage::ColorTarget)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			optimizedClearValue.Color[0] = texParams.m_clear.m_color.r;
			optimizedClearValue.Color[1] = texParams.m_clear.m_color.g;
			optimizedClearValue.Color[2] = texParams.m_clear.m_color.b;
			optimizedClearValue.Color[3] = texParams.m_clear.m_color.a;
		}

		if (texParams.m_usage & re::Texture::Usage::DepthTarget)
		{
			SEAssert(texture.GetNumMips() == 1,
				"Depth target cannot have mips. Note: Depth-Stencil formats support mipmaps, arrays, and multiple "
				"planes. See https://learn.microsoft.com/en-us/windows/win32/direct3d12/subresources");

			flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			optimizedClearValue.DepthStencil.Depth = texParams.m_clear.m_depthStencil.m_depth;
			optimizedClearValue.DepthStencil.Stencil = texParams.m_clear.m_depthStencil.m_stencil;

			if (texture.HasInitialData())
			{
				initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			}
		}

		const uint32_t numMips = texture.GetNumMips();

		D3D12_RESOURCE_DESC resourceDesc{};
		switch (texParams.m_dimension)
		{
		case re::Texture::Dimension::Texture1D:
		case re::Texture::Dimension::Texture1DArray:
		{
			SEAssert(texParams.m_height == 1, "Invalid height for a 1D texture");

			resourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(
				texPlatParams->m_format,
				texParams.m_width,
				texParams.m_arraySize,
				numMips,
				flags,
				D3D12_TEXTURE_LAYOUT_UNKNOWN,	// layout
				0);								// alignment
		}
		break;
		case re::Texture::Dimension::Texture2D:
		case re::Texture::Dimension::Texture2DArray:
		{
			resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				texPlatParams->m_format,
				texParams.m_width,
				texParams.m_height,
				texParams.m_arraySize,
				numMips,
				1,								// sampleCount
				0,								// sampleQuality
				flags,
				D3D12_TEXTURE_LAYOUT_UNKNOWN,	// layout
				0);								// alignment
		}
		break;
		case re::Texture::Dimension::Texture3D:
		{
			resourceDesc = CD3DX12_RESOURCE_DESC::Tex3D(
				texPlatParams->m_format,
				texParams.m_width,
				texParams.m_height,
				texParams.m_arraySize,			// Number of depth slices
				numMips,
				flags,
				D3D12_TEXTURE_LAYOUT_UNKNOWN,	// layout
				0);								// alignment
		}
		break;
		case re::Texture::Dimension::TextureCube:
		case re::Texture::Dimension::TextureCubeArray:
		{
			resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				texPlatParams->m_format,
				texParams.m_width,
				texParams.m_height,
				texParams.m_arraySize * 6,
				numMips,
				1,								// sampleCount
				0,								// sampleQuality
				flags,
				D3D12_TEXTURE_LAYOUT_UNKNOWN,	// layout
				0);								// alignment
		}
		break;
		default:
			SEAssertF("Invalid texture dimension");
		}

		dx12::HeapManager& heapMgr = re::Context::GetAs<dx12::Context*>()->GetHeapManager();

		texPlatParams->m_gpuResource = heapMgr.CreateResource(dx12::ResourceDesc{
				.m_resourceDesc = resourceDesc,
				.m_optimizedClearValue = optimizedClearValue,
				.m_heapType = D3D12_HEAP_TYPE_DEFAULT,
				.m_initialState = initialState,
				.m_isMSAATexture = (texParams.m_multisampleMode == re::Texture::MultisampleMode::Enabled)},
			texture.GetWName().c_str());

		return initialState;
	}


	void UpdateTopLevelSubresources(
		dx12::CommandList* copyCmdList, re::Texture const* texture, ID3D12Resource* intermediate)
	{
		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		const uint32_t numBytesPerFace = texture->GetTotalBytesPerFace();

		// Texture3Ds have a single subresource per mip level, regardless of their depth
		const uint32_t arraySize = texParams.m_dimension == re::Texture::Texture3D ? 1 : texParams.m_arraySize;
		const uint8_t numFaces = re::Texture::GetNumFaces(texture);

		for (uint32_t arrayIdx = 0; arrayIdx < arraySize; arrayIdx++)
		{
			for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
			{
				// Note: We currently assume we only have data for the first mip of each face
				const uint32_t mipIdx = 0;
				const uint32_t intermediateByteOffset = ((arrayIdx * numFaces) + faceIdx) * numBytesPerFace;

				copyCmdList->UpdateSubresource(
					texture, 
					arrayIdx, 
					faceIdx,
					mipIdx,
					intermediate,
					intermediateByteOffset);
			}
		}
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
		case re::Texture::Format::R32_UINT:
		{
			return DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
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
		case re::Texture::Format::R16_UNORM:
		{
			return DXGI_FORMAT::DXGI_FORMAT_R16_UNORM;
		}
		break;
		case re::Texture::Format::RGBA8_UNORM: // 8 bits per channel x N channels
		{
			return texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ?
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		break;
		case re::Texture::Format::RG8_UNORM:
		{
			return DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM;
		}
		break;
		case re::Texture::Format::R8_UNORM:
		{
			return DXGI_FORMAT::DXGI_FORMAT_R8_UNORM;
		}
		break;
		case re::Texture::Format::R8_UINT:
		{
			return DXGI_FORMAT::DXGI_FORMAT_R8_UINT;
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


	// Returns DXGI_FORMAT_UNKNOWN if no equivalent UAV-compatible format is known
	DXGI_FORMAT Texture::GetEquivalentUAVCompatibleFormat(DXGI_FORMAT format)
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


	Texture::PlatformParams::PlatformParams(re::Texture const& texture)
		: m_srvDescriptors(dx12::DescriptorCache::DescriptorType::SRV)
		, m_uavDescriptors(dx12::DescriptorCache::DescriptorType::UAV)
		, m_rtvDescriptors(dx12::DescriptorCache::DescriptorType::RTV)
		, m_dsvDescriptors(dx12::DescriptorCache::DescriptorType::DSV)
	{
		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		m_format = GetTextureFormat(texParams);
	}


	Texture::PlatformParams::~PlatformParams()
	{
		SEAssert(m_gpuResource == nullptr && m_format == DXGI_FORMAT_UNKNOWN,
			"dx12::Texture::PlatformParams::~PlatformParams() called before Destroy()");
	}


	void Texture::PlatformParams::Destroy()
	{
		m_format = DXGI_FORMAT_UNKNOWN;
		m_gpuResource = nullptr;

		m_srvDescriptors.Destroy();
		m_uavDescriptors.Destroy();

		m_rtvDescriptors.Destroy();
		m_dsvDescriptors.Destroy();
	}


	// -----------------------------------------------------------------------------------------------------------------


	void Texture::Create(re::Texture& texture, dx12::CommandList* copyCmdList)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert(texPlatParams->m_isCreated == false, "Texture is already created");
		texPlatParams->m_isCreated = true;

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		SEAssert(texParams.m_usage > 0 && texParams.m_usage != re::Texture::Usage::Invalid,
			"Invalid texture usage");
		
		SEAssert((texParams.m_usage & re::Texture::Usage::DepthTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::DepthTarget) == 0 ||
			(texParams.m_usage ^ (re::Texture::Usage::DepthTarget | re::Texture::Usage::ColorSrc)) == 0,
			"Invalid depth target usage pattern. A depth target can only be a depth target or source texture");

		SEAssert((texParams.m_usage & re::Texture::Usage::StencilTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::StencilTarget) == 0,
			"Invalid usage stencil target usage pattern. A stencil target can only be a stencil target");

		SEAssert((texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::DepthStencilTarget) == 0,
			"Invalid depth stencil usage pattern. A depth stencil target can only be a depth stencil target");

		SEAssert((texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0, "TODO: Support depth stencil targets");
		SEAssert((texParams.m_usage & re::Texture::Usage::StencilTarget) == 0, "TODO: Support stencil targets");
		
		const bool needsSimultaneousAccess = SimultaneousAccessIsNeeded(texParams);

		// Figure out our resource needs:
		const bool needsUAV = UAVIsNeeded(texParams, texPlatParams->m_format);
		const uint32_t numMips = texture.GetNumMips();
		const uint32_t numSubresources = texture.GetTotalNumSubresources();

		SEAssert(((texParams.m_usage & re::Texture::Usage::SwapchainColorProxy) == 0 &&
			(texParams.m_usage & re::Texture::Usage::DepthTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::StencilTarget) == 0) ||
			numMips == 1,
			"Current texture usage type cannot have MIPs");
		SEAssert((texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0,
			"TODO: Support depth-stencil textures (which do support mips, arrays, and multiple planes)");

		// D3D12 Initial resource states:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#initial-states-for-resources
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

		// Create a committed resource:
		if ((texParams.m_usage & re::Texture::Usage::SwapchainColorProxy) == 0)
		{
			initialState = CreateTextureResource(texture, needsUAV, needsSimultaneousAccess);
		}

		// Upload initial data via an intermediate upload heap:
		if ((texParams.m_usage & re::Texture::Usage::ColorSrc) && texture.HasInitialData())
		{
			const uint8_t numFaces = re::Texture::GetNumFaces(&texture);
			const uint8_t bytesPerTexel = re::Texture::GetNumBytesPerTexel(texParams.m_format);
			const uint32_t numBytesPerFace = static_cast<uint32_t>(texture.GetTotalBytesPerFace());
			const uint32_t totalBytes = texParams.m_arraySize * numFaces * numBytesPerFace;
			SEAssert(totalBytes > 0 &&
				totalBytes == texParams.m_arraySize * numFaces * texParams.m_width * texParams.m_height * bytesPerTexel,
				"Texture sizes don't make sense");
			
			// Note: If we don't request an intermediate buffer large enough, the UpdateSubresources call will return 0
			// and no update is actually recorded on the command list.
			// Buffers have the same size on all adapters: The smallest multiple of 64KB >= the buffer width
			// See remarks here:
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getresourceallocationinfo(uint_uint_constd3d12_resource_desc)

			const uint32_t intermediateBufferWidth = util::RoundUpToNearestMultiple(
				totalBytes, 
				static_cast<uint32_t>(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

			dx12::HeapManager& heapMgr = re::Context::GetAs<dx12::Context*>()->GetHeapManager();

			// GPUResources automatically use a deferred deletion, it is safe to let this go out of scope immediately
			std::wstring const& intermediateName = texture.GetWName() + L" intermediate buffer";
			std::unique_ptr<dx12::GPUResource> intermediateResource = heapMgr.CreateResource(dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(intermediateBufferWidth),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ, },
				intermediateName.c_str());

			UpdateTopLevelSubresources(copyCmdList, &texture, intermediateResource->Get());
		}

		texPlatParams->m_isDirty = false;
	}


	// re::Texture::Create factory wrapper, for the DX12-specific case where we need to create a Texture resource using
	// an existing ID3D12Resource
	std::shared_ptr<re::Texture> Texture::CreateFromExistingResource(
		std::string const& name, 
		re::Texture::TextureParams const& params, 
		Microsoft::WRL::ComPtr<ID3D12Resource> textureResource)
	{
		SEAssert((params.m_usage & re::Texture::Usage::SwapchainColorProxy),
			"Invalid/unexpected texture format. For now, this function is used to create a backbuffer color target");

		// Note: re::Texture::Create will enroll the texture in API object creation, and eventually call the standard 
		// dx12::Texture::Create above
		std::shared_ptr<re::Texture> newTexture = re::Texture::Create(name, params);

		dx12::Texture::PlatformParams* texPlatParams = 
			newTexture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert(!texPlatParams->m_gpuResource, "Texture is already created");

		texPlatParams->m_gpuResource = std::make_unique<dx12::GPUResource>(
			textureResource, D3D12_RESOURCE_STATE_COMMON, util::ToWideString(name).c_str());

		return newTexture;
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetSRV(re::Texture const* tex, re::TextureView const& texView)
	{
		SEAssert(tex, "Texture cannot be null");

		dx12::Texture::PlatformParams const* texPlatParams = 
			tex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		return texPlatParams->m_srvDescriptors.GetCreateDescriptor(tex, texView);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetUAV(re::Texture const* tex, re::TextureView const& texView)
	{
		SEAssert(tex, "Texture cannot be null");

		dx12::Texture::PlatformParams const* texPlatParams =
			tex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		return texPlatParams->m_uavDescriptors.GetCreateDescriptor(tex, texView);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRTV(re::Texture const* tex, re::TextureView const& texView)
	{
		SEAssert(tex, "Texture cannot be null");

		dx12::Texture::PlatformParams const* texPlatParams =
			tex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		return texPlatParams->m_rtvDescriptors.GetCreateDescriptor(tex, texView);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDSV(re::Texture const* tex, re::TextureView const& texView)
	{
		SEAssert(tex, "Texture cannot be null");

		dx12::Texture::PlatformParams const* texPlatParams =
			tex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		return texPlatParams->m_dsvDescriptors.GetCreateDescriptor(tex, texView);
	}


	void Texture::Destroy(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		texPlatParams->m_gpuResource = nullptr;
	}


	void Texture::ShowImGuiWindow(re::Texture const& texture, float scale)
	{
		ImGui::Text("TODO: Support ImGui texture previews in DX12");
	}
}