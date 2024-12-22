// © 2023 Adam Badke. All rights reserved.
#include "Buffer_DX12.h"
#include "BufferAllocator_DX12.h"
#include "CommandList_DX12.h"
#include "Context_DX12.h"
#include "RenderManager_DX12.h"

#include "Core/ProfilingMarkers.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/MathUtils.h"
#include "Core/Util/TextUtils.h"

#include <d3dx12.h>


namespace dx12
{
	void BufferAllocator::GetSubAllocation(
		re::Buffer::UsageMask usageMask,
		uint64_t alignedSize, 
		uint64_t& heapOffsetOut,
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
			(allocationPool == re::BufferAllocator::VertexStream &&
				alignedSize % 16 == 0),
			"Invalid alignment");

		resourcePtrOut = m_singleFrameBufferResources[allocationPool][writeIdx]->Get();

		// Our heap offset is the base index of the stack we've allocated for each Type
		heapOffsetOut = AdvanceBaseIdx(allocationPool, util::CheckedCast<uint32_t>(alignedSize));
	}


	void BufferAllocator::Initialize(uint64_t currentFrame)
	{
		SEStaticAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
			"Fixed allocation size must match the default resource placement alignment");

		re::BufferAllocator::Initialize(currentFrame);

		const uint8_t numBuffers = re::RenderManager::GetNumFramesInFlight();
		for (uint8_t i = 0; i < re::BufferAllocator::AllocationPool_Count; ++i)
		{
			m_singleFrameBufferResources[i].resize(numBuffers);
		}

		dx12::HeapManager& heapMgr = re::Context::GetAs<dx12::Context*>()->GetHeapManager();

		for (uint8_t bufferIdx = 0; bufferIdx < m_numFramesInFlight; bufferIdx++)
		{	
			m_singleFrameBufferResources[re::BufferAllocator::Constant][bufferIdx] = heapMgr.CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ,
				},
				util::ToWideString(std::format("Shared constant buffer committed resource {}", bufferIdx)).c_str() );

			m_singleFrameBufferResources[re::BufferAllocator::Structured][bufferIdx] = heapMgr.CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ,
				},
				util::ToWideString(std::format("Shared structured buffer committed resource {}", bufferIdx)).c_str());

			m_singleFrameBufferResources[re::BufferAllocator::VertexStream][bufferIdx] = heapMgr.CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
					.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
					.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ,
				},
				util::ToWideString(std::format("Shared vertex buffer committed resource {}", bufferIdx)).c_str());
		}
	}


	void BufferAllocator::BufferDataPlatform()
	{
		// Note: BufferAllocator::m_dirtyBuffersForPlatformUpdateMutex is already locked by this point

		if (!m_dirtyBuffersForPlatformUpdate.empty())
		{
			dx12::Context* context = re::Context::GetAs<dx12::Context*>();
			dx12::CommandQueue* copyQueue = &context->GetCommandQueue(dx12::CommandListType::Copy);

			SEBeginGPUEvent(
				copyQueue->GetD3DCommandQueue(),
				perfmarkers::Type::CopyQueue, 
				"Copy Queue: Update default heap buffers");

			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();

			// Record our updates:
			SEBeginCPUEvent("dx12::BufferAllocator::BufferDataPlatform: dx12::Buffer::Update(s)");
			for (auto const& entry : m_dirtyBuffersForPlatformUpdate)
			{
				dx12::Buffer::Update(
					entry.m_buffer,
					entry.m_baseOffset,
					entry.m_numBytes,
					copyCommandList.get());
			}
			SEEndCPUEvent();

			SEBeginCPUEvent("dx12::BufferAllocator::BufferDataPlatform: Execute copy queue");
			copyQueue->Execute(1, &copyCommandList);
			SEEndCPUEvent();

			SEEndGPUEvent(copyQueue->GetD3DCommandQueue());
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