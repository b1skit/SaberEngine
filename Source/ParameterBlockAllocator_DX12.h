// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "ParameterBlockAllocator.h"


namespace dx12
{
	class ParameterBlockAllocator
	{
	public:
		struct PlatformParams final : public re::ParameterBlockAllocator::PlatformParams
		{
			// Constant buffer shared committed resources:
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_sharedConstantBufferResources;
			
			// Structured buffer shared committed resources:
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_sharedStructuredBufferResources;
		};


		static void GetSubAllocation(
			re::ParameterBlock::PBDataType,
			uint64_t alignedSize, 
			uint64_t& heapOffsetOut,
			Microsoft::WRL::ComPtr<ID3D12Resource>& resourcePtrOut);


	public:
		static void Create(re::ParameterBlockAllocator&);
		static void Destroy(re::ParameterBlockAllocator&);
	};
}