// © 2023 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "Context_DX12.h"
#include "MathUtils.h"
#include "Buffer_DX12.h"
#include "BufferAllocator_DX12.h"
#include "RenderManager_DX12.h"
#include "TextUtils.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h


namespace dx12
{
	void BufferAllocator::GetSubAllocation(
		re::Buffer::DataType dataType, 
		uint64_t alignedSize, 
		uint64_t& heapOffsetOut,
		Microsoft::WRL::ComPtr<ID3D12Resource>& resourcePtrOut)
	{
		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		dx12::BufferAllocator::PlatformParams* baPlatParams =
			ba.GetPlatformParams()->As<dx12::BufferAllocator::PlatformParams*>();

		const uint8_t writeIdx = baPlatParams->GetWriteIndex();

		switch (dataType)
		{
		case re::Buffer::DataType::SingleElement:
		{
			SEAssert(alignedSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0, "Invalid alignment");
			resourcePtrOut = baPlatParams->m_sharedConstantBufferResources[writeIdx];
		}
		break;
		case re::Buffer::DataType::Array:
		{
			SEAssert(alignedSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0, "Invalid alignment");
			resourcePtrOut = baPlatParams->m_sharedStructuredBufferResources[writeIdx];
		}
		break;
		default: SEAssertF("Invalid DataType");
		}

		// Our heap offset is the base index of the stack we've allocated for each DataType
		heapOffsetOut = baPlatParams->AdvanceBaseIdx(dataType, util::CheckedCast<uint32_t>(alignedSize));
	}


	void BufferAllocator::Create(re::BufferAllocator& ba)
	{
		// Note: DX12 supports double or triple buffering. Currently we're using a hard-coded triple buffer, but we
		// don't need to. We clear the buffer we're writing to at the beginning of each new frame to ensure its
		// contents are no longer in use

		dx12::BufferAllocator::PlatformParams* baPlatformParams =
			ba.GetPlatformParams()->As<dx12::BufferAllocator::PlatformParams*>();

		baPlatformParams->m_sharedConstantBufferResources.resize(baPlatformParams->m_numBuffers, nullptr);
		baPlatformParams->m_sharedStructuredBufferResources.resize(baPlatformParams->m_numBuffers, nullptr);

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		SEAssert(re::BufferAllocator::k_fixedAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
			"Fixed allocation size must match the default resource placement alignment");

		const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC resourceDesc = 
			CD3DX12_RESOURCE_DESC::Buffer(re::BufferAllocator::k_fixedAllocationByteSize);

		for (uint8_t bufferIdx = 0; bufferIdx < baPlatformParams->m_numBuffers; bufferIdx++)
		{	
			HRESULT hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for constant buffers
				IID_PPV_ARGS(&baPlatformParams->m_sharedConstantBufferResources[bufferIdx]));
			CheckHResult(hr, "Failed to create committed resource");

			baPlatformParams->m_sharedConstantBufferResources[bufferIdx]->SetName(
				util::ToWideString(std::format("Shared constant buffer committed resource {}", bufferIdx)).c_str());

			hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for constant buffers
				IID_PPV_ARGS(&baPlatformParams->m_sharedStructuredBufferResources[bufferIdx]));
			CheckHResult(hr, "Failed to create committed resource");

			baPlatformParams->m_sharedStructuredBufferResources[bufferIdx]->SetName(
				util::ToWideString(std::format("Shared structured buffer committed resource {}", bufferIdx)).c_str());
		}
	}


	void BufferAllocator::Destroy(re::BufferAllocator& ba)
	{
		dx12::BufferAllocator::PlatformParams* baPlatformParams =
			ba.GetPlatformParams()->As<dx12::BufferAllocator::PlatformParams*>();

		SEAssert(baPlatformParams->m_sharedConstantBufferResources.size() == baPlatformParams->m_sharedStructuredBufferResources.size() &&
			baPlatformParams->m_numBuffers == baPlatformParams->m_sharedConstantBufferResources.size() &&
			baPlatformParams->m_numBuffers == dx12::RenderManager::GetNumFramesInFlight(),
			"Mismatched number of single frame buffers");

		baPlatformParams->m_sharedConstantBufferResources.assign(baPlatformParams->m_numBuffers, nullptr);
		baPlatformParams->m_sharedStructuredBufferResources.assign(baPlatformParams->m_numBuffers, nullptr);
	}
}