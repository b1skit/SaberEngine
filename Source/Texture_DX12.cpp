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

		// By now, we know a UAV is possible. Return true for any case where it's actually needed
		
		const bool isComputeTarget = (texParams.m_usage & re::Texture::Usage::ComputeTarget) != 0;
		if (isComputeTarget)
		{
			return true;
		}

		// We generate MIPs in DX12 via a compute shader
		const bool usesMips = texParams.m_useMIPs;
		if (usesMips)
		{
			return true;
		}

		// TODO: We'll need to check multisampling is disabled here, once it's implemented

		// We didn't hit a case where a UAV is explicitely needed
		return false;
	}


	void CreateUAV(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		SEAssert("The texture resource has not been created yet", texPlatParams->m_textureResource != nullptr);
		SEAssert("A UAV has already been created. This is unexpected",
			texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::UAV].empty());

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		// We create a UAV for every MIP:
		const uint32_t numSubresources = texture.GetNumMips();
		texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::UAV].reserve(numSubresources);

		for (size_t mipIdx = 0; mipIdx < numSubresources; mipIdx++)
		{
			texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::UAV].emplace_back(std::move(
				context->GetCPUDescriptorHeapMgr(dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(1)));

			dx12::DescriptorAllocation const& uavDescriptorAllocation =
				texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::UAV].back();

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


	void CreateSRV(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		SEAssert("The texture resource has not been created yet", texPlatParams->m_textureResource != nullptr);
		SEAssert("An SRV has already been created. This is unexpected",
			texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::SRV].empty());

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::SRV].emplace_back(std::move(
			context->GetCPUDescriptorHeapMgr(dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(1)));

		dx12::DescriptorAllocation& srvDescriptorAllocation =
			texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::SRV].back();

		const uint32_t numSubresources = texture.GetNumMips();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = texPlatParams->m_format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (texParams.m_faces == 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			srvDesc.Texture2D = D3D12_TEX2D_SRV
			{
				.MostDetailedMip = 0,
				.MipLevels = numSubresources,
				.PlaneSlice = 0,			// Index in a multi-plane format
				.ResourceMinLODClamp = 0.0f // Allow access to all MIP levels
			};
		}
		else
		{
			SEAssert("We're currently expecting this to be a cubemap",
				texParams.m_faces == 6 && texParams.m_dimension == re::Texture::Dimension::TextureCubeMap);
			
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			
			srvDesc.TextureCube = D3D12_TEXCUBE_SRV
			{
				.MostDetailedMip = 0,
				.MipLevels = numSubresources,
				.ResourceMinLODClamp = 0.0f // Allow access to all MIP levels
			};
		}

		device->CreateShaderResourceView(
			texPlatParams->m_textureResource.Get(),
			&srvDesc,
			srvDescriptorAllocation.GetBaseDescriptor());
	}


	// Returns the initial state
	D3D12_RESOURCE_STATES CreateTextureCommittedResource(re::Texture& texture, bool needsUAV)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture resource already created", texPlatParams->m_textureResource == nullptr);
		
		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		// We'll update these settings for each type of texture resource:	
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_NONE;
		if (needsUAV)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		// TODO: use D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS for color textures (unless MSAA enabled)?

		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = texPlatParams->m_format;

		D3D12_CLEAR_VALUE* optimizedClearValuePtr = &optimizedClearValue;
		// Note: optimizedClearValuePtr must be null unless:
		// - D3D12_RESOURCE_DESC::Dimension is D3D12_RESOURCE_DIMENSION_BUFFER,
		// - D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET or D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL are set in flags

		if ((texParams.m_usage & re::Texture::Usage::ColorTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::DepthTarget) == 0)
		{
			optimizedClearValuePtr = nullptr;
		}

		if (texParams.m_usage & re::Texture::Usage::ColorTarget)
		{
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			optimizedClearValue.Color[0] = texParams.m_clear.m_color.r;
			optimizedClearValue.Color[1] = texParams.m_clear.m_color.g;
			optimizedClearValue.Color[2] = texParams.m_clear.m_color.b;
			optimizedClearValue.Color[3] = texParams.m_clear.m_color.a;
		}

		if (texParams.m_usage & re::Texture::Usage::DepthTarget)
		{
			SEAssert("Depth target cannot have mips", texture.GetNumMips() == 1);
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			optimizedClearValue.DepthStencil.Depth = texParams.m_clear.m_depthStencil.m_depth;
			optimizedClearValue.DepthStencil.Stencil = texParams.m_clear.m_depthStencil.m_stencil;
		}

		D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			texPlatParams->m_format,
			texParams.m_width,
			texParams.m_height,
			texParams.m_faces,		// arraySize
			texture.GetNumMips(),	// mipLevels
			1,						// sampleCount
			0,						// sampleQuality
			flags);

		// Resources can be implicitely promoted to COPY/SOURCE/COPY_DEST from COMMON, and decay to COMMON after
		// being accessed on a copy queue. For now, we just set the initial state as COMMON for everything until a more
		// complex case arises
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		HRESULT hr = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, // TODO: Query support: Unsupported on older versions of Windows
			&resourceDesc,
			initialState,
			optimizedClearValuePtr, // Optimized clear value: Must be NULL except for buffers, or render/depth-stencil targets
			IID_PPV_ARGS(&texPlatParams->m_textureResource));
		dx12::CheckHResult(hr, "Failed to create texture committed resource");

		// Name our D3D resource:
		texPlatParams->m_textureResource->SetName(texture.GetWName().c_str());

		return initialState;
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

		// Allocate an RTV/DSV for each face:
		m_rtvDsvDescriptors.resize(texParams.m_faces);
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
		ID3D12GraphicsCommandList2* copyCommandList, 
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture is already created", texPlatParams->m_isCreated == false);
		texPlatParams->m_isCreated = true;

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		SEAssert("Invalid texture usage", 
			texParams.m_usage > 0 && texParams.m_usage != re::Texture::Usage::Invalid);
		
		SEAssert("Invalid depth target usage pattern. A depth target can only be a depth target",
			(texParams.m_usage & re::Texture::Usage::DepthTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::DepthTarget) == 0);

		SEAssert("Invalid usage stencil target usage pattern. A stencil target can only be a stencil target",
			(texParams.m_usage & re::Texture::Usage::StencilTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::StencilTarget) == 0);

		SEAssert("Invalid depth stencil usage pattern. A depth stencil target can only be a depth stencil target",
			(texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::DepthStencilTarget) == 0);

		SEAssert("TODO: Support depth stencil targets", (texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0);
		SEAssert("TODO: Support stencil targets", (texParams.m_usage & re::Texture::Usage::StencilTarget) == 0);
		

		const bool needsUAV = UAVIsNeeded(texParams, texPlatParams->m_format);
		const uint32_t numSubresources = texture.GetNumMips();

		SEAssert("Current texture usage type cannot have MIPs",
			((texParams.m_usage & re::Texture::Usage::SwapchainColorProxy) == 0 &&
			(texParams.m_usage & re::Texture::Usage::DepthTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::StencilTarget) == 0) ||
			numSubresources == 1);


		// D3D12 Initial resource states:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#initial-states-for-resources
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

		// Create a committed resource:
		if ((texParams.m_usage & re::Texture::Usage::SwapchainColorProxy) == 0)
		{
			initialState = CreateTextureCommittedResource(texture, needsUAV);
		}

		if (texParams.m_usage & re::Texture::Usage::Color)
		{
			// Create an intermediate upload heap:
			const uint8_t bytesPerTexel = re::Texture::GetNumBytesPerTexel(texParams.m_format);
			const uint32_t numBytes = static_cast<uint32_t>(texture.GetTotalBytesPerFace());
			SEAssert("Color target must have data to buffer",
				numBytes > 0 &&
				numBytes == texParams.m_faces * texParams.m_width * texParams.m_height * bytesPerTexel);

			std::vector<D3D12_SUBRESOURCE_DATA> subresourceData;
			subresourceData.reserve(numSubresources);

			SEAssert("TODO: Support textures with multiple faces", texParams.m_faces);
			const uint32_t faceIdx = 0;

			// We don't have any MIP data yet, so we only need to describe MIP 0
			subresourceData.emplace_back(D3D12_SUBRESOURCE_DATA{
				.pData = texture.GetTexelData(faceIdx),

				// https://github.com/microsoft/DirectXTex/wiki/ComputePitch
				// Row pitch: The number of bytes in a scanline of pixels: bytes-per-pixel * width-of-image
				// - Can be larger than the number of valid pixels due to alignment padding
				.RowPitch = bytesPerTexel * texParams.m_width,

				// Slice pitch: The number of bytes in each depth slice
				// - 1D/2D images: The total size of the image, including alignment padding
				.SlicePitch = numBytes
				});

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

			HRESULT hr = device->CreateCommittedResource(
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
				copyCommandList,
				texPlatParams->m_textureResource.Get(),			// Destination resource
				itermediateBufferResource.Get(),				// Intermediate resource
				0,												// Intermediate offset
				0,												// Index of 1st subresource in the resource
				static_cast<uint32_t>(subresourceData.size()),	// Number of subresources in the subresources array
				subresourceData.data());						// Array of subresource data structs
			SEAssert("UpdateSubresources returned 0 bytes. This is unexpected", bufferSizeResult > 0);

			SEAssert("TODO: Support SRVs/UAVs for textures of different dimensions",
				texParams.m_dimension == re::Texture::Dimension::Texture2D && texParams.m_faces == 1);

			// Released once the copy is done
			intermediateResources.emplace_back(itermediateBufferResource);
		}

		// Create a SRV if it's needed:
		if ((texParams.m_usage & re::Texture::Usage::Color) ||
			(texParams.m_usage & re::Texture::Usage::ColorTarget)) // Assume we'll sample a color target at some point
		{
			CreateSRV(texture);
		}

		// Create a UAV if it's needed:
		if (needsUAV)
		{
			CreateUAV(texture);
		}

		texPlatParams->m_isDirty = true;

		// Register the resource with the global resource state tracker:
		context->GetGlobalResourceStates().RegisterResource(
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
			re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().UnregisterResource(
				texPlatParams->m_textureResource.Get());
		}

		texPlatParams->m_textureResource = nullptr;

		for (size_t i = 0; i < dx12::Texture::View::View_Count; i++)
		{
			texPlatParams->m_viewCpuDescAllocations[i].clear();
		}

		texPlatParams->m_rtvDsvDescriptors.clear();

		texPlatParams->m_isCreated = false;
		texPlatParams->m_isDirty = true;
	}
}