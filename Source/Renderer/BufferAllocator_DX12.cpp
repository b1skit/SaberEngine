// © 2023 Adam Badke. All rights reserved.
#include "Buffer_DX12.h"
#include "BufferAllocator_DX12.h"
#include "CommandList_DX12.h"
#include "Context_DX12.h"
#include "RenderManager_DX12.h"

#include "Core/ProfilingMarkers.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/TextUtils.h"


namespace dx12
{
	void BufferAllocator::GetSubAllocation(
		re::Buffer::UsageMask usageMask,
		uint64_t alignedSize, 
		uint64_t& baseByteOffsetOut,
		ID3D12Resource*& resourcePtrOut)
	{
		const uint8_t writeIdx = GetSingleFrameGPUWriteIndex();

		const re::BufferAllocator::AllocationPool allocationPool =
			re::BufferAllocator::BufferUsageMaskToAllocationPool(usageMask);

		SEAssert(allocationPool != re::BufferAllocator::Constant || alignedSize <= 4096 * sizeof(glm::vec4),
			"Constant buffers can only hold up to 4096 float4's");

		SEAssert((allocationPool == re::BufferAllocator::Constant && 
			alignedSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0) ||
			(allocationPool == re::BufferAllocator::Structured &&
				alignedSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0) ||
			(allocationPool == re::BufferAllocator::Raw &&
				alignedSize % 16 == 0),
			"Invalid alignment");

		resourcePtrOut = m_singleFrameBufferResources[allocationPool][writeIdx]->Get();

		// Our heap offset is the base index of the stack we've allocated for each Type
		baseByteOffsetOut = AdvanceBaseIdx(allocationPool, util::CheckedCast<uint32_t>(alignedSize));
	}


	void BufferAllocator::Initialize(uint64_t currentFrame)
	{
		SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
			"Fixed allocation size must match the default resource placement alignment");

		re::BufferAllocator::Initialize(currentFrame);

		const uint8_t numBuffers = re::RenderManager::Get()->GetNumFramesInFlight();
		for (uint8_t i = 0; i < re::BufferAllocator::AllocationPool_Count; ++i)
		{
			m_singleFrameBufferResources[i].resize(numBuffers);
		}

		dx12::HeapManager& heapMgr = re::RenderManager::Get()->GetContext()->As<dx12::Context*>()->GetHeapManager();

		// Note: We must start in the common state to ensure all command list types are able to transition the resource
		constexpr D3D12_RESOURCE_STATES k_initialSharedResourceState = D3D12_RESOURCE_STATE_COMMON;

		for (uint8_t bufferIdx = 0; bufferIdx < m_numFramesInFlight; bufferIdx++)
		{	
			m_singleFrameBufferResources[re::BufferAllocator::Constant][bufferIdx] = heapMgr.CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = k_initialSharedResourceState,
				},
				util::ToWideString(std::format("Shared constant buffer committed resource {}", bufferIdx)).c_str() );

			m_singleFrameBufferResources[re::BufferAllocator::Structured][bufferIdx] = heapMgr.CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = k_initialSharedResourceState,
				},
				util::ToWideString(std::format("Shared structured buffer committed resource {}", bufferIdx)).c_str());

			m_singleFrameBufferResources[re::BufferAllocator::Raw][bufferIdx] = heapMgr.CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = k_initialSharedResourceState,
				},
				util::ToWideString(std::format("Shared vertex buffer committed resource {}", bufferIdx)).c_str());
		}
	}


	void BufferAllocator::BufferDefaultHeapDataPlatform(
		std::vector<PlatformCommitMetadata> const& dirtyBuffersForPlatformUpdate,
		uint8_t frameOffsetIdx)
	{
		if (!dirtyBuffersForPlatformUpdate.empty())
		{
			dx12::Context* context = re::RenderManager::Get()->GetContext()->As<dx12::Context*>();
			dx12::CommandQueue* copyQueue = &context->GetCommandQueue(dx12::CommandListType::Copy);

			SEBeginGPUEvent(copyQueue->GetD3DCommandQueue().Get(),
				perfmarkers::Type::CopyQueue, 
				"Copy Queue: Update default heap buffers");

			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();

			re::GPUTimer::Handle copyTimer = context->GetGPUTimer().StartCopyTimer(
				copyCommandList->GetD3DCommandList().Get(),
				"Copy buffers",
				re::RenderManager::k_GPUFrameTimerName);

			// Allocate a single intermediate resource for all buffer uploads:
			uint64_t totalAlignedBytes = 0;
			for (auto const& entry : dirtyBuffersForPlatformUpdate)
			{
				const uint32_t alignment = dx12::Buffer::GetAlignment(
					re::BufferAllocator::BufferUsageMaskToAllocationPool(entry.m_buffer->GetUsageMask()));

				totalAlignedBytes = util::RoundUpToNearestMultiple<uint64_t>(totalAlignedBytes, alignment);

				totalAlignedBytes += dx12::Buffer::GetAlignedSize(
					entry.m_buffer->GetBufferParams().m_usageMask,
					entry.m_numBytes);
			}
			
			// GPUResources automatically use a deferred deletion, it is safe to let this go out of scope immediately
			dx12::HeapManager& heapMgr = context->GetHeapManager();
			std::unique_ptr<dx12::GPUResource> intermediateResource = heapMgr.CreateResource(dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(totalAlignedBytes),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ, },
				L"Intermediate GPU buffer resource");

			// Record our updates:
			SEBeginCPUEvent("dx12::BufferAllocator::BufferDefaultHeapDataPlatform: dx12::Buffer::Update(s)");

			uint64_t intermediateAlignedBaseOffset = 0;
			for (auto const& entry : dirtyBuffersForPlatformUpdate)
			{
				const uint32_t alignment = dx12::Buffer::GetAlignment(
					re::BufferAllocator::BufferUsageMaskToAllocationPool(entry.m_buffer->GetUsageMask()));

				intermediateAlignedBaseOffset =
					util::RoundUpToNearestMultiple<uint64_t>(intermediateAlignedBaseOffset, alignment);

				SEAssert(intermediateAlignedBaseOffset + entry.m_numBytes <= totalAlignedBytes,
					"Base offset and number of bytes will overflow");

				dx12::Buffer::Update(
					entry.m_buffer,
					frameOffsetIdx,
					entry.m_baseOffset,
					entry.m_numBytes,
					copyCommandList.get(),
					intermediateResource.get(),
					intermediateAlignedBaseOffset);

				// Update the base offset:
				intermediateAlignedBaseOffset +=
					dx12::Buffer::GetAlignedSize(entry.m_buffer->GetBufferParams().m_usageMask, entry.m_numBytes);
			}
			SEEndCPUEvent();

			copyTimer.StopTimer(copyCommandList->GetD3DCommandList().Get());

			SEBeginCPUEvent("dx12::BufferAllocator::BufferDefaultHeapDataPlatform: Execute copy queue");
			copyQueue->Execute(1, &copyCommandList);
			SEEndCPUEvent();

			SEEndGPUEvent(copyQueue->GetD3DCommandQueue().Get());
		}
	}


	void BufferAllocator::Destroy()
	{
		for (uint8_t i = 0; i < re::BufferAllocator::AllocationPool_Count; ++i)
		{
			m_singleFrameBufferResources[i].clear();
		}

		re::BufferAllocator::Destroy();
	}
}