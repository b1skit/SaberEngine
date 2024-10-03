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

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void BufferAllocator::GetSubAllocation(
		re::Buffer::Type dataType, 
		uint64_t alignedSize, 
		uint64_t& heapOffsetOut,
		Microsoft::WRL::ComPtr<ID3D12Resource>& resourcePtrOut)
	{
		const uint8_t writeIdx = GetWriteIndex();

		switch (dataType)
		{
		case re::Buffer::Type::Constant:
		{
			SEAssert(alignedSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0, "Invalid alignment");
			resourcePtrOut = m_sharedConstantBufferResources[writeIdx];
		}
		break;
		case re::Buffer::Type::Structured:
		{
			SEAssert(alignedSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0, "Invalid alignment");
			resourcePtrOut = m_sharedStructuredBufferResources[writeIdx];
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		// Our heap offset is the base index of the stack we've allocated for each Type
		heapOffsetOut = AdvanceBaseIdx(dataType, util::CheckedCast<uint32_t>(alignedSize));
	}


	void BufferAllocator::Initialize(uint64_t currentFrame)
	{
		re::BufferAllocator::Initialize(currentFrame);

		const uint8_t numBuffers = re::RenderManager::GetNumFramesInFlight();
		m_sharedConstantBufferResources.resize(numBuffers, nullptr);
		m_sharedStructuredBufferResources.resize(numBuffers, nullptr);
		
		m_intermediateResourceFenceVals.resize(numBuffers);
		m_intermediateResources.resize(numBuffers);

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		SEStaticAssert(re::BufferAllocator::k_fixedAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
			"Fixed allocation size must match the default resource placement alignment");

		const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC resourceDesc = 
			CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_fixedAllocationByteSize);

		for (uint8_t bufferIdx = 0; bufferIdx < m_numFramesInFlight; bufferIdx++)
		{	
			HRESULT hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for constant buffers
				IID_PPV_ARGS(&m_sharedConstantBufferResources[bufferIdx]));
			CheckHResult(hr, "Failed to create committed resource");

			m_sharedConstantBufferResources[bufferIdx]->SetName(
				util::ToWideString(std::format("Shared constant buffer committed resource {}", bufferIdx)).c_str());

			hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the structured buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for structured buffers
				IID_PPV_ARGS(&m_sharedStructuredBufferResources[bufferIdx]));
			CheckHResult(hr, "Failed to create committed resource");

			m_sharedStructuredBufferResources[bufferIdx]->SetName(
				util::ToWideString(std::format("Shared structured buffer committed resource {}", bufferIdx)).c_str());
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

			dx12::RenderManager const* renderMgr = dynamic_cast<dx12::RenderManager const*>(re::RenderManager::Get());
			const uint8_t intermediateIdx = renderMgr->GetIntermediateResourceIdx();

			// Ensure any updates using the intermediate resources created during the previous frame are done
			if (!copyQueue->GetFence().IsFenceComplete(m_intermediateResourceFenceVals[intermediateIdx]))
			{
				copyQueue->CPUWait(m_intermediateResourceFenceVals[intermediateIdx]);
			}
			m_intermediateResources[intermediateIdx].clear();

			// Record our updates:
			for (auto const& entry : m_dirtyBuffersForPlatformUpdate)
			{
				dx12::Buffer::Update(
					entry.m_buffer,
					entry.m_baseOffset,
					entry.m_numBytes,
					copyCommandList.get(), 
					m_intermediateResources[intermediateIdx]);
			}

			m_intermediateResourceFenceVals[intermediateIdx] = copyQueue->Execute(1, &copyCommandList);

			SEEndGPUEvent(copyQueue->GetD3DCommandQueue());
		}
	}


	void BufferAllocator::Destroy()
	{
		SEAssert(m_sharedConstantBufferResources.size() == m_sharedStructuredBufferResources.size() &&
			m_numFramesInFlight == m_sharedConstantBufferResources.size() &&
			m_numFramesInFlight == dx12::RenderManager::GetNumFramesInFlight(),
			"Mismatched number of single frame buffers");

		m_sharedConstantBufferResources.assign(m_numFramesInFlight, nullptr);
		m_sharedStructuredBufferResources.assign(m_numFramesInFlight, nullptr);

		re::BufferAllocator::Destroy();
	}
}