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
			PlatformParams();
			~PlatformParams();

			void Destroy() override;

			bool GPUResourceIsValid() const;

			ID3D12Resource* const& GetGPUResource() const; // Get the resolved GPU resource

			D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;
			uint64_t GetHeapByteOffset() const; // Debug only: GetGPUVirtualAddress() automatically applies this


		public:
			std::vector<ReadbackResource> m_readbackResources; // CPU readback
			uint8_t m_currentMapFrameLatency; // Used to compute the resource index during unmapping

		
		private:
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

		private:
			// May be invalid (e.g. single-frame buffers in shared resource). Use m_resolvedGPUResource instead
			friend class Buffer;
			std::unique_ptr<dx12::GPUResource> m_gpuResource;

			ID3D12Resource* m_resolvedGPUResource; // Use this instead of m_gpuResource

			// For multiple resources sub-allocated from a single GPUResource
			// i.e. Single-frame buffers suballocated from the stack, or mutabable buffers with N-buffered allocations
			uint64_t m_heapByteOffset;
		};


	public:
		static void Create(re::Buffer&);
		static void Update(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);
		
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


	inline ID3D12Resource* const& Buffer::PlatformParams::GetGPUResource() const
	{
		return m_resolvedGPUResource;
	}


	inline D3D12_GPU_VIRTUAL_ADDRESS Buffer::PlatformParams::GetGPUVirtualAddress() const
	{
		return GetGPUResource()->GetGPUVirtualAddress() + GetHeapByteOffset();
	}


	inline uint64_t Buffer::PlatformParams::GetHeapByteOffset() const
	{
		return m_heapByteOffset;
	}


	// ---


	inline D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetSRV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, buffer->GetBufferParams()) ||
			re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer->GetBufferParams()),
			"Buffer is missing the Structured usage bit");
		SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, buffer->GetBufferParams()),
			"SRV buffers must have GPU reads enabled");

		dx12::Buffer::PlatformParams const* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		SEAssert(bufferPlatParams->GetHeapByteOffset() == 0, "Unexpected heap byte offset");

		return bufferPlatParams->m_srvDescriptors.GetCreateDescriptor(buffer, view);
	}


	inline D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetUAV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, buffer->GetBufferParams()) ||
			re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer->GetBufferParams()),
			"Buffer is missing the Structured usage bit");
		SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPUWrite, buffer->GetBufferParams()),
			"UAV buffers must have GPU writes enabled");

		dx12::Buffer::PlatformParams const* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		SEAssert(bufferPlatParams->GetHeapByteOffset() == 0, "Unexpected heap byte offset");

		return bufferPlatParams->m_uavDescriptors.GetCreateDescriptor(buffer, view);
	}


	inline D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetCBV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, buffer->GetBufferParams()),
			"Buffer is missing the Constant usage bit");

		SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, buffer->GetBufferParams()) &&
			!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, buffer->GetBufferParams()),
			"Invalid usage flags for a constant buffer");

		dx12::Buffer::PlatformParams const* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		return bufferPlatParams->m_cbvDescriptors.GetCreateDescriptor(buffer, view);
	}


	inline D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetGPUVirtualAddress(re::Buffer const* buffer)
	{
		SEAssert(buffer, "Buffer cannot be null");

		return buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>()->GetGPUVirtualAddress();
	}


	SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
		"CBV sizes must be in multiples of 256B");

	SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
		"Structured buffer sizes must be in multiples of 64KB");
}