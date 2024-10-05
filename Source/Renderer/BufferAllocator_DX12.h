// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "BufferAllocator.h"
#include "CPUDescriptorHeapManager_DX12.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class BufferAllocator final : public virtual re::BufferAllocator
	{
	public:
		BufferAllocator() = default;
		~BufferAllocator() override = default;

		void Initialize(uint64_t currentFrame) override;

		void Destroy() override;

		void BufferDataPlatform() override;


	public: // DX12-specific functionality:
		void GetSubAllocation(
			re::Buffer::UsageMask,
			uint64_t alignedSize,
			uint64_t& heapOffsetOut,
			Microsoft::WRL::ComPtr<ID3D12Resource>& resourcePtrOut);


	private:
		std::array<std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>, 
			re::BufferAllocator::AllocationPool_Count> m_singleFrameBufferResources;

		std::vector<uint64_t> m_intermediateResourceFenceVals;
		std::vector<std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>> m_intermediateResources;
	};
}