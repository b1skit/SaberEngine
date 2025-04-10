// © 2025 Adam Badke. All rights reserved.
#include "BindlessResource.h"
#include "BindlessResource_DX12.h"
#include "AccelerationStructure_DX12.h"
#include "BufferView.h"
#include "Buffer_DX12.h"
#include "Texture_DX12.h"
#include "TextureView.h"

#include "Core/Assert.h"

#include <d3d12.h>


namespace dx12
{
	void dx12::AccelerationStructureResource::GetPlatformResource(
		re::AccelerationStructureResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		// Acceleration structures are created in the D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE state, and
		// cannot be transitioned to any other state
		memset(dest, 0, destByteSize);
	}


	void AccelerationStructureResource::GetDescriptor(
		re::AccelerationStructureResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");
		SEAssert(resource.m_viewType == re::ViewType::SRV, "Unexpected view type");

		dx12::AccelerationStructure::PlatformParams const* tlasPlatParams =
			resource.m_resource->GetPlatformParams()->As<dx12::AccelerationStructure::PlatformParams const*>();

		const D3D12_CPU_DESCRIPTOR_HANDLE tlasSRVHandle = tlasPlatParams->m_tlasSRV.GetBaseDescriptor();

		SEAssert(destByteSize == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE), "Invalid destination size");
		memcpy(dest, &tlasSRVHandle, destByteSize);
	}


	void AccelerationStructureResource::GetResourceUseState(
		re::AccelerationStructureResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		constexpr D3D12_RESOURCE_STATES k_defaultResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		SEAssert(dest && destByteSize == sizeof(D3D12_RESOURCE_STATES), "Invalid destination size");
		memcpy(dest, &k_defaultResourceState, destByteSize);
	}


	// ---


	void dx12::BufferResource::GetPlatformResource(
		re::BufferResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		// Single frame buffers are (currently) stack allocated from a shared heap and thus we don't change state
		const bool isInSharedHeap = resource.m_resource->GetBufferParams().m_lifetime == re::Lifetime::SingleFrame;
		if (isInSharedHeap)
		{
			memset(dest, 0, destByteSize);
		}
		else
		{
			dx12::Buffer::PlatformParams* platParams =
				resource.m_resource->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

			SEAssert(platParams->GetGPUResource(), "Buffer resolved resource is null");

			SEAssert(destByteSize == sizeof(ID3D12Resource*), "Invalid destination size");

			memcpy(dest, &platParams->GetGPUResource(), destByteSize);
		}
	}


	void BufferResource::GetDescriptor(
		re::BufferResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle{};

		switch (resource.m_viewType)
		{
		case re::ViewType::CBV:
		{
			descriptorHandle = dx12::Buffer::GetCBV(
				resource.m_resource.get(),
				re::BufferView(resource.m_resource));
		}
		break;
		case re::ViewType::SRV:
		{
			descriptorHandle = dx12::Buffer::GetSRV(
				resource.m_resource.get(),
				re::BufferView(resource.m_resource));
		}
		break;
		case re::ViewType::UAV:
		{
			descriptorHandle = dx12::Buffer::GetUAV(
				resource.m_resource.get(),
				re::BufferView(resource.m_resource));
		}
		break;
		default: SEAssertF("Invalid view type");
		}

		SEAssert(destByteSize == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE), "Invalid destination size");
		memcpy(dest, &descriptorHandle, destByteSize);
	}


	// ---


	void dx12::TextureResource::GetPlatformResource(
		re::TextureResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		dx12::Texture::PlatformParams* platParams =
			resource.m_resource->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		SEAssert(platParams->m_gpuResource, "Texture GPU resource is null");

		SEAssert(destByteSize == sizeof(ID3D12Resource*), "Invalid destination size");

		ID3D12Resource* textureResource = platParams->m_gpuResource->Get();
		memcpy(dest, &textureResource, destByteSize);
	}


	void TextureResource::GetDescriptor(
		re::TextureResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle{};

		switch (resource.m_viewType)
		{
		case re::ViewType::SRV:
		{
			descriptorHandle = dx12::Texture::GetSRV(
				resource.m_resource,
				re::TextureView(resource.m_resource));
		}
		break;
		case re::ViewType::UAV:
		{
			// Cubemaps must be viewed as a Texture2DArray to create a UAV
			switch (resource.m_resource->GetTextureParams().m_dimension)
			{
			case re::Texture::TextureCube:
			case re::Texture::TextureCubeArray:
						{
				descriptorHandle = dx12::Texture::GetUAV(
					resource.m_resource,
					re::TextureView::Texture2DArrayView());
			}
			break;
			default:
			{
				descriptorHandle = dx12::Texture::GetUAV(
					resource.m_resource,
					re::TextureView(resource.m_resource));
			}
			}
			
		}
		break;
		case re::ViewType::CBV:
		default: SEAssertF("Invalid view type");
		}

		SEAssert(destByteSize == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE), "Invalid destination size");
		memcpy(dest, &descriptorHandle, destByteSize);
	}


	void TextureResource::GetResourceUseState(
		re::TextureResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

		switch (resource.m_viewType)
		{
		case re::ViewType::SRV:
		{
			resourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
		break;
		case re::ViewType::UAV:
		{
			resourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		break;
		case re::ViewType::CBV:
		default: SEAssertF("Invalid view type");
		}
		
		SEAssert(dest && destByteSize == sizeof(D3D12_RESOURCE_STATES), "Invalid destination size");
		memcpy(dest, &resourceState, destByteSize);
	}


	// ---


	void dx12::VertexStreamResource::GetPlatformResource(
		re::VertexStreamResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		// Single frame buffers are (currently) stack allocated from a shared heap and thus we don't change state
		const bool isInSharedHeap = 
			resource.m_resource.GetBuffer()->GetBufferParams().m_lifetime == re::Lifetime::SingleFrame;
		if (isInSharedHeap)
		{
			memset(dest, 0, destByteSize);
		}
		else
		{
			dx12::Buffer::PlatformParams* platParams =
				resource.m_resource.GetBuffer()->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

			SEAssert(platParams->GetGPUResource(), "Buffer resolved resource is null");

			SEAssert(destByteSize == sizeof(ID3D12Resource*), "Invalid destination size");

			memcpy(dest, &platParams->GetGPUResource(), destByteSize);
		}
	}


	void VertexStreamResource::GetDescriptor(
		re::VertexStreamResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle{};

		switch (resource.m_viewType)
		{
		case re::ViewType::SRV:
		{
			descriptorHandle = dx12::Buffer::GetSRV(
				resource.m_resource.GetBuffer(),
				resource.m_resource.m_view);
		}
		break;
		case re::ViewType::CBV:
		case re::ViewType::UAV:
		default: SEAssertF("Invalid view type");
		}

		SEAssert(destByteSize == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE), "Invalid destination size");
		memcpy(dest, &descriptorHandle, destByteSize);
	}
}