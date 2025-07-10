// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "BufferAllocator.h"
#include "HeapManager_DX12.h"


namespace dx12
{
	class BufferAllocator final : public virtual re::BufferAllocator
	{
	public:
		BufferAllocator() = default;
		~BufferAllocator() override = default;

		void InitializeInternal(uint64_t currentFrame, void* heapManager) override;

		void Destroy() override;


	protected:
		void BufferDefaultHeapDataPlatform(std::vector<PlatformCommitMetadata> const&, uint8_t frameOffsetIdx) override;


	public: // DX12-specific functionality:
		void GetSubAllocation(
			re::Buffer::Usage,
			uint64_t alignedSize,
			uint64_t& heapOffsetOut,
			ID3D12Resource*& resourcePtrOut);


	private:
		std::array<std::vector<std::unique_ptr<dx12::GPUResource>>,
			re::BufferAllocator::AllocationPool_Count> m_singleFrameBufferResources;
	};
}