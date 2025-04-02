// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "BufferAllocator.h"
#include "DescriptorCache_DX12.h"
#include "HeapManager_DX12.h"

#include <d3d12.h>


namespace re
{
	class BufferView;
}

namespace dx12
{
	class CommandList;


	class Buffer
	{
	public:
		struct ReadbackResource
		{
			ReadbackResource() = default;
			~ReadbackResource() = default;
			ReadbackResource(ReadbackResource&& rhs) noexcept
			{
				{
					std::scoped_lock lock(m_readbackFenceMutex, rhs.m_readbackFenceMutex);

					m_readbackGPUResource = std::move(rhs.m_readbackGPUResource);
					m_readbackFence = rhs.m_readbackFence;
				}
			}

			std::unique_ptr<dx12::GPUResource> m_readbackGPUResource;

			uint64_t m_readbackFence = 0;
			std::mutex m_readbackFenceMutex;
		};


		struct PlatformParams final : public re::Buffer::PlatformParams
		{
			PlatformParams()
				: m_gpuResource(nullptr)
				, m_resolvedGPUResource(nullptr)
				, m_heapByteOffset(0)
				, m_currentMapFrameLatency(std::numeric_limits<uint8_t>::max())
				, m_srvDescriptors(dx12::DescriptorCache::DescriptorType::SRV)
				, m_uavDescriptors(dx12::DescriptorCache::DescriptorType::UAV)
				, m_cbvDescriptors(dx12::DescriptorCache::DescriptorType::CBV)
				, m_views{0}
			{
			}

			~PlatformParams()
			{
				m_srvDescriptors.Destroy();
				m_uavDescriptors.Destroy();
				m_cbvDescriptors.Destroy();
			}

			bool GPUResourceIsValid() const;

		public:
			ID3D12Resource* m_resolvedGPUResource; // Use this instead of m_gpuResource

			// For multiple resources sub-allocated from a single GPUResource
			// i.e. Single-frame buffers suballocated from the stack, or mutabable buffers with N-buffered allocations
			uint64_t m_heapByteOffset; 

			std::vector<ReadbackResource> m_readbackResources; // CPU readback
			uint8_t m_currentMapFrameLatency; // Used to compute the resource index during unmapping

			mutable dx12::DescriptorCache m_srvDescriptors;
			mutable dx12::DescriptorCache m_uavDescriptors;
			mutable dx12::DescriptorCache m_cbvDescriptors;

		
		private:
			union
			{
				D3D12_INDEX_BUFFER_VIEW  m_indexBufferView;
				D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
			} m_views;
			std::mutex m_viewMutex; // Views created at first usage during command recording


			// May be invalid (e.g. single-frame buffers in shared resource). Use m_resolvedGPUResource instead
			friend class Buffer;
			std::unique_ptr<dx12::GPUResource> m_gpuResource; 
		};


	public:
		static void Create(re::Buffer&);
		static void Update(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);
		static void Destroy(re::Buffer&);
		
		static void const* MapCPUReadback(re::Buffer const&, uint8_t frameLatency);
		static void UnmapCPUReadback(re::Buffer const&);

		// DX12-specific functionality:
		static void Update(
			re::Buffer const*,
			uint32_t baseOffset,
			uint32_t numBytes,
			dx12::CommandList* copyCmdList);

		static D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(re::Buffer const*, re::BufferView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(re::Buffer const*, re::BufferView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetCBV(re::Buffer const*, re::BufferView const&);

		// Index/vertex views:
		static D3D12_INDEX_BUFFER_VIEW const* GetOrCreateIndexBufferView(re::Buffer const&, re::BufferView const&);
		static D3D12_VERTEX_BUFFER_VIEW const* GetOrCreateVertexBufferView(re::Buffer const&, re::BufferView const&);

		static D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(re::Buffer const*);
	};


	inline bool Buffer::PlatformParams::GPUResourceIsValid() const
	{
		return m_gpuResource && m_gpuResource->IsValid();
	}


	SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
		"CBV sizes must be in multiples of 256B");

	SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
		"Structured buffer sizes must be in multiples of 64KB");
}