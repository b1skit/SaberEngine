// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "BufferAllocator.h"
#include "DescriptorCache_DX12.h"
#include "HeapManager_DX12.h"

#include <d3d12.h>


namespace re
{
	struct BufferResource;
	class BufferView;
	struct VertexStreamResource;
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


		struct PlatObj final : public re::Buffer::PlatObj
		{
			PlatObj();
			~PlatObj();

			void Destroy() override;

			bool GPUResourceIsValid() const;

			ID3D12Resource* const& GetGPUResource() const; // Get the resolved GPU resource

			D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;
			uint64_t GetBaseByteOffset() const; // Debug only: GetGPUVirtualAddress() automatically applies this


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
			uint64_t m_baseByteOffset;
		};


	public:
		static void Create(re::Buffer&);
		static void Update(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);
		
		static void const* MapCPUReadback(re::Buffer const&, uint8_t frameLatency);
		static void UnmapCPUReadback(re::Buffer const&);

		// DX12-specific functionality:
		static void Update(
			re::Buffer const*,
			uint8_t frameOffsetIdx,
			uint32_t baseOffset,
			uint32_t numBytes,
			dx12::CommandList* copyCmdList);


	public: // Helper accessors: Prefer using the PlatObj helpers over these
		static D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(re::Buffer const*); // Convenience wrapper for PlatObj::GetGPUVirtualAddress()
		static uint64_t GetAlignedSize(re::Buffer::UsageMask usageMask, uint32_t bufferSize);

		static bool IsInSharedHeap(re::Buffer const*);


	public: // Resource views:
		static D3D12_CPU_DESCRIPTOR_HANDLE GetCBV(re::Buffer const*, re::BufferView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(re::Buffer const*, re::BufferView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(re::Buffer const*, re::BufferView const&);

		// Index/vertex views:
		static D3D12_VERTEX_BUFFER_VIEW const* GetOrCreateVertexBufferView(re::Buffer const&, re::BufferView const&);
		static D3D12_INDEX_BUFFER_VIEW const* GetOrCreateIndexBufferView(re::Buffer const&, re::BufferView const&);


	protected: // Bindless buffer resources: Get a unique descriptor for the Nth frame that respects shared heap offsets
		friend struct BufferResource;
		friend struct VertexStreamResource;
		static D3D12_CPU_DESCRIPTOR_HANDLE GetCBV(re::Buffer const*, re::BufferView const&, uint8_t frameOffsetIdx);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(re::Buffer const*, re::BufferView const&, uint8_t frameOffsetIdx);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(re::Buffer const*, re::BufferView const&, uint8_t frameOffsetIdx);


	private:
		static D3D12_CPU_DESCRIPTOR_HANDLE GetCBVInternal(re::Buffer const*, re::BufferView const&, uint64_t baseByteOffset);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetSRVInternal(re::Buffer const*, re::BufferView const&, uint64_t baseByteOffset);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetUAVInternal(re::Buffer const*, re::BufferView const&, uint64_t baseByteOffset);
	};


	inline bool Buffer::PlatObj::GPUResourceIsValid() const
	{
		return m_gpuResource && m_gpuResource->IsValid();
	}


	inline ID3D12Resource* const& Buffer::PlatObj::GetGPUResource() const
	{
		return m_resolvedGPUResource;
	}


	inline D3D12_GPU_VIRTUAL_ADDRESS Buffer::PlatObj::GetGPUVirtualAddress() const
	{
		return GetGPUResource()->GetGPUVirtualAddress() + GetBaseByteOffset();
	}


	inline uint64_t Buffer::PlatObj::GetBaseByteOffset() const
	{
		return m_baseByteOffset;
	}


	// ---


	inline D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetGPUVirtualAddress(re::Buffer const* buffer)
	{
		SEAssert(buffer, "Buffer cannot be null");

		return buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>()->GetGPUVirtualAddress();
	}


	inline bool Buffer::IsInSharedHeap(re::Buffer const* buffer)
	{
		return buffer->GetBufferParams().m_lifetime == re::Lifetime::SingleFrame;
	}


	inline D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetCBV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();

		return GetCBVInternal(buffer, view, platObj->GetBaseByteOffset());
	}


	inline D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetSRV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();

		return GetSRVInternal(buffer, view, platObj->GetBaseByteOffset());
	}


	inline D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetUAV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();

		return GetUAVInternal(buffer, view, platObj->GetBaseByteOffset());
	}


	SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
		"CBV sizes must be in multiples of 256B");

	SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
		"Structured buffer sizes must be in multiples of 64KB");
}