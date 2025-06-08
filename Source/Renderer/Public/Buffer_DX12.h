// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "BufferView.h"
#include "BufferAllocator.h"
#include "DescriptorCache_DX12.h"
#include "EnumTypes.h"
#include "HeapManager_DX12.h"


namespace re
{
	struct BufferResource;
	struct VertexStreamResource;
}

namespace dx12
{
	class CommandList;


	class Buffer
	{
	public:
		struct ReadbackResource final
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
			D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(re::BufferView const&) const;
			D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(re::BufferInput const&) const;
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
			dx12::CommandList* copyCmdList,
			dx12::GPUResource* intermediateResource,
			uint64_t itermediateBaseOffset);


	public: // Helper accessors: Prefer using the PlatObj helpers over these
		static D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(re::Buffer const*); // Convenience wrapper for PlatObj::GetGPUVirtualAddress()
		static uint64_t GetAlignedSize(re::Buffer::UsageMask usageMask, uint32_t bufferSize);
		static constexpr uint32_t GetAlignment(re::BufferAllocator::AllocationPool);

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


	inline D3D12_GPU_VIRTUAL_ADDRESS Buffer::PlatObj::GetGPUVirtualAddress(re::BufferView const& view) const
	{
		uint32_t firstElement = 0;
		uint32_t structuredByteStride = 0;

		if (view.IsVertexStreamView())
		{
			firstElement = view.m_streamView.m_firstElement;
			structuredByteStride = re::DataTypeToByteStride(view.m_streamView.m_dataType);
		}
		else
		{
			firstElement = view.m_bufferView.m_firstElement;
			structuredByteStride = view.m_bufferView.m_structuredByteStride;
		}

		const uint32_t firstElementOffset = firstElement * structuredByteStride;

		return GetGPUVirtualAddress() + firstElementOffset;
	}

	
	inline D3D12_GPU_VIRTUAL_ADDRESS Buffer::PlatObj::GetGPUVirtualAddress(re::BufferInput const& bufferInput) const
	{
		return GetGPUVirtualAddress(bufferInput.GetView());
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


	inline constexpr uint32_t Buffer::GetAlignment(re::BufferAllocator::AllocationPool allocationPool)
	{
		switch (allocationPool)
		{
		case re::BufferAllocator::Constant: return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256B
		case re::BufferAllocator::Structured: return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // 64KB
		case re::BufferAllocator::Raw: return 16; // Minimum alignment of a float4 is 16B
		case re::BufferAllocator::AllocationPool_Count:
		default:
			SEAssertF("Invalid buffer data type");
		}
		return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // This should never happen
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