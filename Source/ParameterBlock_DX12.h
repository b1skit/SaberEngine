// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "ParameterBlock.h"
#include "ParameterBlockAllocator.h"


namespace dx12
{
	class ParameterBlock
	{
	public:
		struct PlatformParams final : public re::ParameterBlock::PlatformParams
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> m_resource = nullptr;
			uint64_t m_heapByteOffset = 0;

			// TODO: We currently only set ParameterBlocks inline in the root signature...
			DescriptorAllocation m_cpuDescAllocation;
		};


	public:
		static void Create(re::ParameterBlock& paramBlock);
		static void Update(re::ParameterBlock const& paramBlock);
		static void Destroy(re::ParameterBlock& paramBlock);
	};


	// CBV sizes must be in multiples of 256B
	static_assert(re::ParameterBlockAllocator::k_fixedAllocationByteSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);

	// Structured buffer sizes must be in multiples of 64KB
	static_assert(re::ParameterBlockAllocator::k_fixedAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0);
}