// © 2024 Adam Badke. All rights reserved.
#include "Buffer_DX12.h"
#include "Buffer.h"
#include "BufferView.h"
#include "Context_DX12.h"
#include "DescriptorCache_DX12.h"
#include "Sampler.h"
#include "Texture.h"
#include "Texture_DX12.h"

#include "Core/Assert.h"
#include "Core/Util/MathUtils.h"

#include <d3dx12.h>


namespace
{
	dx12::CPUDescriptorHeapManager::HeapType DescriptorTypeToHeapType(
		dx12::DescriptorCache::DescriptorType descriptorType)
	{
		switch (descriptorType)
		{
		case dx12::DescriptorCache::DescriptorType::SRV:
		case dx12::DescriptorCache::DescriptorType::UAV: 
		case dx12::DescriptorCache::DescriptorType::CBV:
			return dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV;
		case dx12::DescriptorCache::DescriptorType::RTV: return dx12::CPUDescriptorHeapManager::HeapType::RTV;
		case dx12::DescriptorCache::DescriptorType::DSV: return dx12::CPUDescriptorHeapManager::HeapType::DSV;
		default: SEAssertF("Invalid descriptor type");
		}
		return dx12::CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV; // This should never happen
	}


	inline DXGI_FORMAT GetTextureSRVFormat(core::InvPtr<re::Texture> const& texture)
	{
		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		switch (texPlatParams->m_format)
		{
		case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		default: return texPlatParams->m_format;
		}
	}


	void InitializeTextureSRV(
		dx12::DescriptorAllocation& descriptor,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& texView)
	{
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		dx12::Texture::PlatformParams const* texPlatParams = 
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = GetTextureSRVFormat(texture);
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		switch (texView.m_viewDimension)
		{
		case re::Texture::Texture1D:
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			srvDesc.Texture1D = D3D12_TEX1D_SRV{
				.MostDetailedMip = texView.Texture1D.m_firstMip,
				.MipLevels = texView.Texture1D.m_mipLevels,
				.ResourceMinLODClamp = texView.Texture1D.m_resoureceMinLODClamp};
		}
		break;
		case re::Texture::Texture1DArray:
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			srvDesc.Texture1DArray = D3D12_TEX1D_ARRAY_SRV{
				.MostDetailedMip = texView.Texture1DArray.m_firstMip,
				.MipLevels = texView.Texture1DArray.m_mipLevels,
				.FirstArraySlice = texView.Texture1DArray.m_firstArraySlice,
				.ArraySize = texView.Texture1DArray.m_arraySize,
				.ResourceMinLODClamp = texView.Texture1DArray.m_resoureceMinLODClamp };
		}
		break;
		case re::Texture::Texture2D:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D = D3D12_TEX2D_SRV{
					.MostDetailedMip = texView.Texture2D.m_firstMip,
					.MipLevels = texView.Texture2D.m_mipLevels,
					.PlaneSlice = texView.Texture2D.m_planeSlice,
					.ResourceMinLODClamp = texView.Texture2D.m_resoureceMinLODClamp };
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}
		}
		break;
		case re::Texture::Texture2DArray:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV{
					.MostDetailedMip = texView.Texture2DArray.m_firstMip,
					.MipLevels = texView.Texture2DArray.m_mipLevels,
					.FirstArraySlice = texView.Texture2DArray.m_firstArraySlice,
					.ArraySize = texView.Texture2DArray.m_arraySize,
					.PlaneSlice = texView.Texture2DArray.m_planeSlice,
					.ResourceMinLODClamp = texView.Texture2DArray.m_resoureceMinLODClamp };
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}
		}
		break;
		case re::Texture::Texture3D:
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Texture3D = D3D12_TEX3D_SRV{
				.MostDetailedMip = texView.Texture3D.m_firstMip,
				.MipLevels = texView.Texture3D.m_mipLevels,
				.ResourceMinLODClamp = texView.Texture3D.m_resoureceMinLODClamp };
		}
		break;
		case re::Texture::TextureCube:
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube = D3D12_TEXCUBE_SRV{ // Allow access to all MIP levels
				.MostDetailedMip = texView.TextureCube.m_firstMip,
				.MipLevels = texView.TextureCube.m_mipLevels,
				.ResourceMinLODClamp = texView.TextureCube.m_resoureceMinLODClamp };
		}
		break;
		case re::Texture::TextureCubeArray:
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			srvDesc.TextureCubeArray = D3D12_TEXCUBE_ARRAY_SRV{
				.MostDetailedMip = texView.TextureCubeArray.m_firstMip,
				.MipLevels = texView.TextureCubeArray.m_mipLevels,
				.First2DArrayFace = texView.TextureCubeArray.m_first2DArrayFace,
				.NumCubes = texView.TextureCubeArray.m_numCubes,
				.ResourceMinLODClamp = texView.TextureCubeArray.m_resoureceMinLODClamp };
		}
		break;
		default: SEAssertF("Invalid dimension");
		}

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		device->CreateShaderResourceView(texPlatParams->m_gpuResource->Get(), &srvDesc, descriptor.GetBaseDescriptor());
	}


	void InitializeTextureUAV(
		dx12::DescriptorAllocation& descriptor,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& texView)
	{
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		dx12::Texture::PlatformParams const* texPlatParams = 
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		const DXGI_FORMAT uavCompatibleFormat = dx12::Texture::GetEquivalentUAVCompatibleFormat(texPlatParams->m_format);
		SEAssert(uavCompatibleFormat != DXGI_FORMAT_UNKNOWN, "Failed to find equivalent UAV-compatible format");

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = uavCompatibleFormat;

		switch (texView.m_viewDimension)
		{
		case re::Texture::Texture1D:
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			uavDesc.Texture1D = D3D12_TEX1D_UAV{
				.MipSlice = texView.Texture1D.m_firstMip};
		}
		break;
		case re::Texture::Texture1DArray:
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			uavDesc.Texture1DArray = D3D12_TEX1D_ARRAY_UAV{
				.MipSlice = texView.Texture1DArray.m_firstMip,
				.FirstArraySlice = texView.Texture1DArray.m_firstArraySlice,
				.ArraySize = texView.Texture1DArray.m_arraySize};
		}
		break;
		case re::Texture::Texture2D:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D = D3D12_TEX2D_UAV{
					.MipSlice = texView.Texture2D.m_firstMip,
					.PlaneSlice = texView.Texture2D.m_planeSlice};
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}			
		}
		break;
		case re::Texture::Texture2DArray:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray = D3D12_TEX2D_ARRAY_UAV{
					.MipSlice = texView.Texture2DArray.m_firstMip,
					.FirstArraySlice = texView.Texture2DArray.m_firstArraySlice,
					.ArraySize = texView.Texture2DArray.m_arraySize,
					.PlaneSlice = texView.Texture2DArray.m_planeSlice};
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}
		}
		break;
		case re::Texture::Texture3D:
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D = D3D12_TEX3D_UAV{
				.MipSlice = texView.Texture3D.m_firstMip,
				.FirstWSlice = texView.Texture3D.m_firstWSlice,
				.WSize = texView.Texture3D.m_wSize };
		}
		break;
		case re::Texture::TextureCube:
		case re::Texture::TextureCubeArray:
		{
			SEAssertF("Invalid view dimension: Cubemaps must be viewed as a Texture2DArray to create a UAV");
		}
		break;
		default: SEAssertF("Invalid dimension");
		}

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		device->CreateUnorderedAccessView(
			texPlatParams->m_gpuResource->Get(),
			nullptr,		// Counter resource
			&uavDesc,
			descriptor.GetBaseDescriptor());
	}


	void InitializeTextureRTV(
		dx12::DescriptorAllocation& descriptor,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& texView)
	{
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = texPlatParams->m_format;

		switch (texView.m_viewDimension)
		{
		case re::Texture::Texture1D:
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			rtvDesc.Texture1D = D3D12_TEX1D_RTV{
				.MipSlice = texView.Texture1D.m_firstMip };
		}
		break;
		case re::Texture::Texture1DArray:
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			rtvDesc.Texture1DArray = D3D12_TEX1D_ARRAY_RTV{
				.MipSlice = texView.Texture1DArray.m_firstMip,
				.FirstArraySlice = texView.Texture1DArray.m_firstArraySlice,
				.ArraySize = texView.Texture1DArray.m_arraySize };
		}
		break;
		case re::Texture::Texture2D:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				rtvDesc.Texture2D = D3D12_TEX2D_RTV{
					.MipSlice = texView.Texture2D.m_firstMip,
					.PlaneSlice = texView.Texture2D.m_planeSlice };
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}
		}
		break;
		case re::Texture::Texture2DArray:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV{
					.MipSlice = texView.Texture2DArray.m_firstMip,
					.FirstArraySlice = texView.Texture2DArray.m_firstArraySlice,
					.ArraySize = texView.Texture2DArray.m_arraySize,
					.PlaneSlice = texView.Texture2DArray.m_planeSlice };
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}
		}
		break;
		case re::Texture::Texture3D:
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			rtvDesc.Texture3D = D3D12_TEX3D_RTV{
				.MipSlice = texView.Texture3D.m_firstMip,
				.FirstWSlice = texView.Texture3D.m_firstWSlice,
				.WSize = texView.Texture3D.m_wSize };
		}
		break;
		case re::Texture::TextureCube:
		case re::Texture::TextureCubeArray:
		{
			SEAssertF("Invalid view dimension: Cubemaps must be viewed as a Texture2DArray to create a RTV");
		}
		break;
		default: SEAssertF("Invalid dimension");
		}

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		device->CreateRenderTargetView(
			texPlatParams->m_gpuResource->Get(),
			&rtvDesc,
			descriptor.GetBaseDescriptor());

	}


	void InitializeTextureDSV(
		dx12::DescriptorAllocation& descriptor,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& texView)
	{
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = texPlatParams->m_format;

		dsvDesc.Flags = static_cast<D3D12_DSV_FLAGS>(texView.Flags.m_depthStencil);
		SEAssert(texView.StencilWritesEnabled(), "TODO: Support stencil buffers");

		switch (texView.m_viewDimension)
		{
		case re::Texture::Texture1D:
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			dsvDesc.Texture1D = D3D12_TEX1D_DSV{
				.MipSlice = texView.Texture1D.m_firstMip
			};
		}
		break;
		case re::Texture::Texture1DArray:
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			dsvDesc.Texture1DArray = D3D12_TEX1D_ARRAY_DSV{
				.MipSlice = texView.Texture1DArray.m_firstMip,
				.FirstArraySlice = texView.Texture1DArray.m_firstArraySlice,
				.ArraySize = texView.Texture1DArray.m_arraySize};
		}
		break;
		case re::Texture::Texture2D:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Texture2D = D3D12_TEX2D_DSV{
					.MipSlice = texView.Texture2D.m_firstMip};
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}
		}
		break;
		case re::Texture::Texture2DArray:
		{
			switch (texParams.m_multisampleMode)
			{
			case re::Texture::MultisampleMode::Disabled:
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_DSV{
					.MipSlice = texView.Texture2DArray.m_firstMip,
					.FirstArraySlice = texView.Texture2DArray.m_firstArraySlice,
					.ArraySize = texView.Texture2DArray.m_arraySize };
			}
			break;
			case re::Texture::MultisampleMode::Enabled:
			{
				SEAssertF("TODO: Support multisampling");
			}
			break;
			default: SEAssertF("Invalid multisample mode");
			}
		}
		break;
		case re::Texture::Texture3D:
		{
			SEAssertF("Invalid view dimension: Texture3D cannot be used with depth views");
		}
		break;
		case re::Texture::TextureCube:
		case re::Texture::TextureCubeArray:
		{
			SEAssertF("Invalid view dimension: Cubemaps must be viewed as a Texture2DArray to create a DSV");
		}
		break;
		default: SEAssertF("Invalid dimension");
		}

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		device->CreateDepthStencilView(
			texPlatParams->m_gpuResource->Get(),
			&dsvDesc,
			descriptor.GetBaseDescriptor());
	}


	// ---


	void InitializeBufferSRV(
		dx12::DescriptorAllocation& descriptor,
		re::Buffer const& buffer,
		re::BufferView const& bufView)
	{
		re::Buffer::BufferParams const& bufferParams = buffer.GetBufferParams();

		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
			.Format = DXGI_FORMAT_UNKNOWN, // Assume we're creating a view of a structured buffer
			.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer = D3D12_BUFFER_SRV{
				.FirstElement = bufView.m_buffer.m_firstElement,
				.NumElements = bufView.m_buffer.m_numElements,
				.StructureByteStride = bufView.m_buffer.m_structuredByteStride, // Size of 1 element in the shader
				.Flags = D3D12_BUFFER_SRV_FLAGS::D3D12_BUFFER_SRV_FLAG_NONE,
			}};

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		device->CreateShaderResourceView(params->m_resovedGPUResource, &srvDesc, descriptor.GetBaseDescriptor());
	}


	void InitializeBufferUAV(
		dx12::DescriptorAllocation& descriptor,
		re::Buffer const& buffer,
		re::BufferView const& bufView)
	{
		re::Buffer::BufferParams const& bufferParams = buffer.GetBufferParams();

		const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc
		{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
			.Buffer = D3D12_BUFFER_UAV{
				.FirstElement = bufView.m_buffer.m_firstElement,
				.NumElements = bufView.m_buffer.m_numElements,
				.StructureByteStride = bufView.m_buffer.m_structuredByteStride, // Size of the struct in the shader
				.CounterOffsetInBytes = 0,
				.Flags = D3D12_BUFFER_UAV_FLAG_NONE,
			}
		};

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		device->CreateUnorderedAccessView(
			params->m_resovedGPUResource,
			nullptr,	// Optional counter resource
			&uavDesc,
			descriptor.GetBaseDescriptor());
	}


	void InitializeBufferCBV(
		dx12::DescriptorAllocation& descriptor,
		re::Buffer const& buffer,
		re::BufferView const& bufView)
	{
		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		const D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc
		{
			.BufferLocation = params->m_resovedGPUResource->GetGPUVirtualAddress(),
			.SizeInBytes = util::RoundUpToNearestMultiple<uint32_t>(
				buffer.GetTotalBytes(),
				D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT),
		};
		
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		device->CreateConstantBufferView(
			&cbvDesc,
			descriptor.GetBaseDescriptor());
	}
}

namespace dx12
{
	DescriptorCache::DescriptorCache(DescriptorType descriptorType)
		: m_descriptorType(descriptorType)
	{
		SEAssert(m_descriptorType != DescriptorType::DescriptorType_Count, "Invalid descriptor type");
	}


	DescriptorCache::~DescriptorCache()
	{
		{
			std::lock_guard<std::mutex> lock(m_descriptorCacheMutex);

			SEAssert(m_descriptorCache.empty() && m_descriptorType == DescriptorType::DescriptorType_Count,
				"~DescriptorCache() called before Destroy()");
		}
	}


	void DescriptorCache::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_descriptorCacheMutex);

			for (auto& cacheEntry : m_descriptorCache)
			{
				// Descriptor cache is destroyed via deferred texture/buffer deletion; It's safe to immediately free here
				cacheEntry.second.Free(0); 
			}
			m_descriptorCache.clear();
			m_descriptorType = DescriptorType::DescriptorType_Count;
		}
	}


	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorCache::GetCreateDescriptor(
		core::InvPtr<re::Texture> const& texture, re::TextureView const& texView)
	{
		re::TextureView::ValidateView(texture, texView); // _DEBUG only

		SEAssert(texView.m_viewDimension != re::Texture::Dimension::Dimension_Invalid, "Invalid view dimension");

		{
			std::lock_guard<std::mutex> lock(m_descriptorCacheMutex);

			std::vector<CacheEntry>::iterator cacheItr = m_descriptorCache.end();
			if (m_descriptorCache.empty())
			{
				m_descriptorCache.reserve(texture->GetTotalNumSubresources());
				cacheItr = m_descriptorCache.end();
			}
			else
			{
				cacheItr = std::lower_bound( // Get an iterator to the 1st element >= our new texture view data hash
					m_descriptorCache.begin(),
					m_descriptorCache.end(),
					texView.GetDataHash(),
					CacheComparator());
			}

			// If no cache entries are >= our new data hash, or the one we found doesn't match, create a new descriptor
			if (cacheItr == m_descriptorCache.end() || cacheItr->first != texView.GetDataHash())
			{
				dx12::Context* context = re::Context::GetAs<dx12::Context*>();
				ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

				CacheEntry newCacheEntry{
					texView.GetDataHash(),
					context->GetCPUDescriptorHeapMgr(DescriptorTypeToHeapType(m_descriptorType)).Allocate(1) };

				switch (m_descriptorType)
				{
				case DescriptorType::SRV:
				{
					InitializeTextureSRV(newCacheEntry.second, texture, texView);
				}
				break;
				case DescriptorType::UAV:
				{
					InitializeTextureUAV(newCacheEntry.second, texture, texView);
				}
				break;
				case DescriptorType::RTV:
				{
					InitializeTextureRTV(newCacheEntry.second, texture, texView);
				}
				break;
				case DescriptorType::DSV:
				{
					InitializeTextureDSV(newCacheEntry.second, texture, texView);
				}
				break;
				default: SEAssertF("Invalid heap type");
				}

				auto insertResult = m_descriptorCache.insert(
					std::upper_bound(
						m_descriptorCache.begin(),
						m_descriptorCache.end(),
						newCacheEntry,
						CacheComparator()),
					std::move(newCacheEntry)
				);

				return insertResult->second.GetBaseDescriptor();
			}
			else
			{
				return cacheItr->second.GetBaseDescriptor();
			}
		}
	}


	// ---


	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorCache::GetCreateDescriptor(
		re::Buffer const& buffer, re::BufferView const& bufView)
	{
		{
			std::lock_guard<std::mutex> lock(m_descriptorCacheMutex);

			const util::DataHash bufViewHash = bufView.GetDataHash();

			std::vector<CacheEntry>::iterator cacheItr = m_descriptorCache.end();
			if (m_descriptorCache.empty())
			{
				m_descriptorCache.reserve(buffer.GetArraySize()); // A guess
				cacheItr = m_descriptorCache.end();
			}
			else
			{
				cacheItr = std::lower_bound( // Get an iterator to the 1st element >= our new texture view data hash
					m_descriptorCache.begin(),
					m_descriptorCache.end(),
					bufViewHash,
					CacheComparator());
			}

			// If no cache entries are >= our new data hash, or the one we found doesn't match, create a new descriptor
			if (cacheItr == m_descriptorCache.end() || cacheItr->first != bufViewHash)
			{
				dx12::Context* context = re::Context::GetAs<dx12::Context*>();
				ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

				CacheEntry newCacheEntry{
					bufViewHash,
					context->GetCPUDescriptorHeapMgr(DescriptorTypeToHeapType(m_descriptorType)).Allocate(1) };

				switch (m_descriptorType)
				{
				case DescriptorType::SRV:
				{
					InitializeBufferSRV(newCacheEntry.second, buffer, bufView);
				}
				break;
				case DescriptorType::UAV:
				{
					InitializeBufferUAV(newCacheEntry.second, buffer, bufView);
				}
				break;
				case DescriptorType::CBV:
				{
					InitializeBufferCBV(newCacheEntry.second, buffer, bufView);
				}
				break;
				case DescriptorType::RTV:
				case DescriptorType::DSV:
				{
					SEAssertF("Invalid heap type for a re::Buffer");
				}
				break;
				default: SEAssertF("Invalid heap type");
				}

				auto insertResult = m_descriptorCache.insert(
					std::upper_bound(
						m_descriptorCache.begin(),
						m_descriptorCache.end(),
						newCacheEntry,
						CacheComparator()),
					std::move(newCacheEntry)
				);

				return insertResult->second.GetBaseDescriptor();
			}
			else
			{
				return cacheItr->second.GetBaseDescriptor();
			}
		}
	}


	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorCache::GetCreateDescriptor(
		re::Buffer const* buffer, re::BufferView const& bufView)
	{
		SEAssert(buffer, "Trying to get a descriptor for a null buffer");
		return GetCreateDescriptor(*buffer, bufView);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorCache::GetCreateDescriptor(
		std::shared_ptr<re::Buffer const> const& buffer, re::BufferView const& bufView)
	{
		SEAssert(buffer, "Trying to get a descriptor for a null buffer");
		return GetCreateDescriptor(*buffer.get(), bufView);
	}
}