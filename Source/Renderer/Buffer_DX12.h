// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CPUDescriptorHeapManager_DX12.h"
#include "Buffer.h"
#include "BufferAllocator.h"

#include <d3d12.h>
#include <wrl.h>


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

					m_resource = std::move(rhs.m_resource);
					m_readbackFence = rhs.m_readbackFence;
				}
			}

			Microsoft::WRL::ComPtr<ID3D12Resource> m_resource = nullptr;

			uint64_t m_readbackFence = 0;
			std::mutex m_readbackFenceMutex;
		};


		struct PlatformParams final : public re::Buffer::PlatformParams
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> m_resource = nullptr;
			uint64_t m_heapByteOffset = 0;

			DescriptorAllocation m_uavCPUDescAllocation; // Used for GPU-writable immutable buffers

			std::vector<ReadbackResource> m_readbackResources; // CPU readback
			uint8_t m_currentMapFrameLatency; // Used to compute the resource index during unmapping
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
			dx12::CommandList* copyCmdList,
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources);
	};


	SEStaticAssert(re::BufferAllocator::k_fixedAllocationByteSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
		"CBV sizes must be in multiples of 256B");

	SEStaticAssert(re::BufferAllocator::k_fixedAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
		"Structured buffer sizes must be in multiples of 64KB");
}