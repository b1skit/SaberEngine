// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "Assert.h"
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


	bool SRVIsNeeded(re::Texture::TextureParams const& texParams)
	{
		return (texParams.m_usage & re::Texture::Usage::Color);
	}


	bool SimultaneousAccessIsNeeded(re::Texture::TextureParams const& texParams)
	{
		// Assume that if a resource is used as a target and anything else, it could be used simultaneously
		const bool usedAsInputAndTArget = 
			((texParams.m_usage & re::Texture::Usage::ColorTarget) &&
			(texParams.m_usage ^ re::Texture::Usage::ColorTarget)) ||
			((texParams.m_usage & re::Texture::Usage::ComputeTarget) &&
				(texParams.m_usage ^ re::Texture::Usage::ComputeTarget));
		if (!usedAsInputAndTArget)
		{
			return false;
		}

		// As per the documentation, simultaneous access cannot be used with buffers, MSAA textures, or when the
		// D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL flag is used
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
		if (texParams.m_mipMode == re::Texture::MipMode::AllocateGenerate)
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
			texPlatParams->m_uavCpuDescAllocations.empty());

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		const uint32_t numMips = texture.GetNumMips();

		const uint32_t numSubresources = texture.GetTotalNumSubresources();
		SEAssert("Unexpected number of subresources", numSubresources == numMips * texParams.m_faces);

		texPlatParams->m_uavCpuDescAllocations.reserve(numSubresources);

		// We create a UAV for every MIP, for each face:
		for (uint32_t faceIdx = 0; faceIdx < texParams.m_faces; faceIdx++)
		{
			for (uint32_t mipIdx = 0; mipIdx < numMips; mipIdx++)
			{
				texPlatParams->m_uavCpuDescAllocations.emplace_back(std::move(
					context->GetCPUDescriptorHeapMgr(dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(1)));

				dx12::DescriptorAllocation const& uavDescriptorAllocation =
					texPlatParams->m_uavCpuDescAllocations.back();

				const DXGI_FORMAT uavCompatibleFormat = GetEquivalentUAVCompatibleFormat(texPlatParams->m_format);
				SEAssert("Failed to find equivalent UAV-compatible format", uavCompatibleFormat != DXGI_FORMAT_UNKNOWN);

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
				uavDesc.Format = uavCompatibleFormat;

				switch (texParams.m_dimension)
				{
				case re::Texture::Dimension::Texture2D:
				{
					SEAssert("Unexpected number of faces", texParams.m_faces == 1);

					uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					
					uavDesc.Texture2D = D3D12_TEX2D_UAV{
							.MipSlice = mipIdx,
							.PlaneSlice = 0 };
				}
				break;
				case re::Texture::Dimension::TextureCubeMap:
				{
					uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;

					uavDesc.Texture2DArray = D3D12_TEX2D_ARRAY_UAV{
						.MipSlice = mipIdx,
						.FirstArraySlice = faceIdx,
						.ArraySize = 1, // Only view one element of our array
						.PlaneSlice = 0 // "Only Plane Slice 0 is valid when creating a view on a non-planar format"
					};
				}
				break;
				default:
					SEAssertF("Invalid texture dimension");
				}

				device->CreateUnorderedAccessView(
					texPlatParams->m_textureResource.Get(),
					nullptr,		// Counter resource
					&uavDesc,
					uavDescriptorAllocation.GetBaseDescriptor());
			}
		}
	}


	DXGI_FORMAT GetSRVFormat(re::Texture const& texture)
	{
		dx12::Texture::PlatformParams const* texPlatParams =
			texture.GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		switch (texPlatParams->m_format)
		{
		case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		default: return texPlatParams->m_format;
		}
	}


	void CreateSRV(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = 
			texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		SEAssert("The texture resource has not been created yet", texPlatParams->m_textureResource != nullptr);
		SEAssert("An SRV has already been created. This is unexpected",
			!texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2D].IsValid() &&
			!texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2DArray].IsValid() &&
			!texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::TextureCubeMap].IsValid());

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		const uint32_t numMips = texture.GetNumMips();

		const DXGI_FORMAT srvFormat = GetSRVFormat(texture);

		if (texParams.m_faces == 1)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = srvFormat;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			srvDesc.Texture2D = D3D12_TEX2D_SRV
			{
				.MostDetailedMip = 0,
				.MipLevels = numMips,
				.PlaneSlice = 0,			// Index in a multi-plane format
				.ResourceMinLODClamp = 0.0f // Allow access to all MIP levels
			};

			texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2D] = std::move(
				context->GetCPUDescriptorHeapMgr(dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(1));

			device->CreateShaderResourceView(
				texPlatParams->m_textureResource.Get(),
				&srvDesc,
				texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2D].GetBaseDescriptor());
		}
		else
		{
			SEAssert("We're currently expecting this to be a cubemap",
				texParams.m_faces == 6 && texParams.m_dimension == re::Texture::Dimension::TextureCubeMap);

			D3D12_SHADER_RESOURCE_VIEW_DESC cubemapSrvDesc{};
			cubemapSrvDesc.Format = srvFormat;
			cubemapSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		
			cubemapSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			
			cubemapSrvDesc.TextureCube = D3D12_TEXCUBE_SRV
			{
				.MostDetailedMip = 0,
				.MipLevels = numMips,
				.ResourceMinLODClamp = 0.0f // Allow access to all MIP levels
			};

			texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::TextureCubeMap] = std::move(
				context->GetCPUDescriptorHeapMgr(dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(1));

			device->CreateShaderResourceView(
				texPlatParams->m_textureResource.Get(),
				&cubemapSrvDesc,
				texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::TextureCubeMap].GetBaseDescriptor());

			// Cubemaps are a special case of a texture array:
			D3D12_SHADER_RESOURCE_VIEW_DESC cubeTexArraySrvDesc{};
			cubeTexArraySrvDesc.Format = srvFormat;
			cubeTexArraySrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			cubeTexArraySrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

			cubeTexArraySrvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV
			{
				.MostDetailedMip = 0,
				.MipLevels = numMips,
				.FirstArraySlice = 0,
				.ArraySize = texParams.m_faces == 6,	// View all 6 faces with a single view
				.PlaneSlice = 0,			// "Only Plane Slice 0 is valid when creating a view on a non-planar format"
				.ResourceMinLODClamp = 0.f,
			};

			texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2DArray] = std::move(
				context->GetCPUDescriptorHeapMgr(dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(1));

			device->CreateShaderResourceView(
				texPlatParams->m_textureResource.Get(),
				&cubeTexArraySrvDesc,
				texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2DArray].GetBaseDescriptor());
		}		
	}


	// Returns the initial state
	D3D12_RESOURCE_STATES CreateTextureCommittedResource(re::Texture& texture, bool needsUAV, bool simultaneousAccess)
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
		if (simultaneousAccess)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
		}

		// Resources can be implicitely promoted to COPY/SOURCE/COPY_DEST from COMMON, and decay to COMMON after
		// being accessed on a copy queue. For now, we (typically) set the initial state as COMMON for everything until
		// more complex cases arise
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

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
			SEAssert("Depth target cannot have mips. Note: Depth-Stencil formats support mipmaps, arrays, and multiple "
				"planes. See https://learn.microsoft.com/en-us/windows/win32/direct3d12/subresources", 
				texture.GetNumMips() == 1);

			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			optimizedClearValue.DepthStencil.Depth = texParams.m_clear.m_depthStencil.m_depth;
			optimizedClearValue.DepthStencil.Stencil = texParams.m_clear.m_depthStencil.m_stencil;

			initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
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

		HRESULT hr = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, // TODO: Query support: Unsupported on older versions of Windows
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


	Texture::PlatformParams::PlatformParams(re::Texture const& texture)
	{
		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		m_format = GetTextureFormat(texParams);

		const uint32_t numMips = texture.GetNumMips();
		const uint32_t numSubresources = texParams.m_faces * numMips;

		if (texParams.m_mipMode == re::Texture::MipMode::None)
		{
			m_uavCpuDescAllocations.reserve(1);
		}
		else
		{
			m_uavCpuDescAllocations.reserve(numSubresources);
		}
	}


	Texture::PlatformParams::~PlatformParams()
	{
		m_format = DXGI_FORMAT_UNKNOWN;
		m_textureResource = nullptr;

		for (uint8_t texDimension = 0; texDimension < re::Texture::Dimension::Dimension_Count; texDimension++)
		{
			m_srvCpuDescAllocations[texDimension].Free(0);
		}
		m_uavCpuDescAllocations.clear();
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
		
		SEAssert("Invalid depth target usage pattern. A depth target can only be a depth target or source texture",
			(texParams.m_usage & re::Texture::Usage::DepthTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::DepthTarget) == 0 ||
			(texParams.m_usage ^ (re::Texture::Usage::DepthTarget | re::Texture::Usage::Color)) == 0);

		SEAssert("Invalid usage stencil target usage pattern. A stencil target can only be a stencil target",
			(texParams.m_usage & re::Texture::Usage::StencilTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::StencilTarget) == 0);

		SEAssert("Invalid depth stencil usage pattern. A depth stencil target can only be a depth stencil target",
			(texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0 ||
			(texParams.m_usage ^ re::Texture::Usage::DepthStencilTarget) == 0);

		SEAssert("TODO: Support depth stencil targets", (texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0);
		SEAssert("TODO: Support stencil targets", (texParams.m_usage & re::Texture::Usage::StencilTarget) == 0);
		
		const bool needsSRV = SRVIsNeeded(texParams);
		const bool needsSimultaneousAccess = SimultaneousAccessIsNeeded(texParams);

		// Figure out our resource needs:
		const bool needsUAV = UAVIsNeeded(texParams, texPlatParams->m_format);
		const uint32_t numMips = texture.GetNumMips();
		const uint32_t numSubresources = texture.GetTotalNumSubresources();

		SEAssert("Current texture usage type cannot have MIPs",
			((texParams.m_usage & re::Texture::Usage::SwapchainColorProxy) == 0 &&
			(texParams.m_usage & re::Texture::Usage::DepthTarget) == 0 &&
			(texParams.m_usage & re::Texture::Usage::StencilTarget) == 0) ||
			numMips == 1);
		SEAssert("TODO: Support depth-stencil textures (which do support mips, arrays, and multiple planes)", 
			(texParams.m_usage & re::Texture::Usage::DepthStencilTarget) == 0);

		// D3D12 Initial resource states:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#initial-states-for-resources
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

		// Create a committed resource:
		if ((texParams.m_usage & re::Texture::Usage::SwapchainColorProxy) == 0)
		{
			initialState = CreateTextureCommittedResource(texture, needsUAV, needsSimultaneousAccess);
		}

		// Upload initial data via an intermediate upload heap:
		if ((texParams.m_usage & re::Texture::Usage::Color) && texture.HasInitialData())
		{
			SEAssert("TODO: Test/support buffering texture data for textures with multiple faces. Initial data for the "
				" first mip of textures with multiple faces probably works, but has not been tested ",
				texParams.m_dimension == re::Texture::Dimension::Texture2D && texParams.m_faces == 1);

			const uint8_t bytesPerTexel = re::Texture::GetNumBytesPerTexel(texParams.m_format);
			const uint32_t numBytesPerFace = static_cast<uint32_t>(texture.GetTotalBytesPerFace());
			const uint32_t totalBytes = numBytesPerFace * texParams.m_faces;
			SEAssert("Texture sizes don't make sense",
				totalBytes > 0 &&
				totalBytes == texParams.m_faces * texParams.m_width * texParams.m_height * bytesPerTexel);
			
			// Note: If we don't request an intermediate buffer large enough, the UpdateSubresources call will return 0
			// and no update is actually recorded on the command list.
			// Buffers have the same size on all adapters: The smallest multiple of 64KB >= the buffer width
			// See remarks here:
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getresourceallocationinfo(uint_uint_constd3d12_resource_desc)
			// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 64KB, as per:
			// https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants

			const uint32_t intermediateBufferWidth = util::RoundUpToNearestMultiple(
				totalBytes, 
				static_cast<uint32_t>(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

			const D3D12_RESOURCE_DESC intermediateBufferResourceDesc =
			{
				.Dimension = D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER,
				.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, // == 64KB, default
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


			// Populate our subresource data
			// Note: We currently assume we only have data for the first mip of each face
			std::vector<D3D12_SUBRESOURCE_DATA> subresourceData;
			subresourceData.reserve(texParams.m_faces);

			for (uint32_t faceIdx = 0; faceIdx < texParams.m_faces; faceIdx++)
			{
				void const* initialData = texture.GetTexelData(faceIdx);
				SEAssert("Initial data cannot be null", initialData);

				subresourceData.emplace_back(D3D12_SUBRESOURCE_DATA{
					.pData = initialData,

					// https://github.com/microsoft/DirectXTex/wiki/ComputePitch
					// Row pitch: The number of bytes in a scanline of pixels: bytes-per-pixel * width-of-image
					// - Can be larger than the number of valid pixels due to alignment padding
					.RowPitch = bytesPerTexel * texParams.m_width,

					// Slice pitch: The number of bytes in each depth slice
					// - 1D/2D images: The total size of the image, including alignment padding
					.SlicePitch = numBytesPerFace
				});
			}

			const uint64_t bufferSizeResult = ::UpdateSubresources(
				copyCommandList,						// Command list
				texPlatParams->m_textureResource.Get(),	// Destination resource
				itermediateBufferResource.Get(),		// Intermediate resource
				0,										// Byte offset to the intermediate resource
				0,										// Index of 1st subresource in the resource
				static_cast<uint32_t>(subresourceData.size()),	// Number of subresources in the subresources array
				subresourceData.data());						// Array of subresource data structs
			SEAssert("UpdateSubresources returned 0 bytes. This is unexpected", bufferSizeResult > 0);

			// Released once the copy is done
			intermediateResources.emplace_back(itermediateBufferResource);
		}

		// Create a SRV if it's needed:
		if (needsSRV)
		{
			CreateSRV(texture);
		}

		// Create a UAV if it's needed:
		if (needsUAV)
		{
			CreateUAV(texture);
		}

		texPlatParams->m_isDirty = false;

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

		// Null out the platform params, and let its destructor clean everything up
		texture.SetPlatformParams(nullptr);
	}
}