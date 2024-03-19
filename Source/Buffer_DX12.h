// � 2022 Adam Badke. All rights reserved.
#pragma once
#include "CPUDescriptorHeapManager_DX12.h"
#include "Buffer.h"
#include "BufferAllocator.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class Buffer
	{
	public:
		struct PlatformParams final : public re::Buffer::PlatformParams
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> m_resource = nullptr;
			uint64_t m_heapByteOffset = 0;

			// TODO: We currently only set Buffers inline in the root signature...
			DescriptorAllocation m_srvCPUDescAllocation;
		};


	public:
		static void Create(re::Buffer&);
		static void Update(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);
		static void Destroy(re::Buffer&);
	};


	// CBV sizes must be in multiples of 256B
	static_assert(re::BufferAllocator::k_fixedAllocationByteSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);

	// Structured buffer sizes must be in multiples of 64KB
	static_assert(re::BufferAllocator::k_fixedAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0);
}