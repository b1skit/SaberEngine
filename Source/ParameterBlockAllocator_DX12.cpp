// © 2023 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "Context_DX12.h"
#include "MathUtils.h"
#include "ParameterBlock_DX12.h"
#include "ParameterBlockAllocator_DX12.h"
#include "RenderManager_DX12.h"
#include "TextUtils.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h


namespace dx12
{
	void ParameterBlockAllocator::GetSubAllocation(
		re::ParameterBlock::PBDataType pbDataType, 
		uint64_t alignedSize, 
		uint64_t& heapOffsetOut,
		Microsoft::WRL::ComPtr<ID3D12Resource>& resourcePtrOut)
	{
		re::ParameterBlockAllocator& pba = re::Context::Get()->GetParameterBlockAllocator();
		dx12::ParameterBlockAllocator::PlatformParams* pbaPlatParams =
			pba.GetPlatformParams()->As<dx12::ParameterBlockAllocator::PlatformParams*>();

		const uint8_t writeIdx = pbaPlatParams->GetWriteIndex();

		switch (pbDataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			SEAssert(alignedSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0, "Invalid alignment");
			resourcePtrOut = pbaPlatParams->m_sharedConstantBufferResources[writeIdx];
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			SEAssert(alignedSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0, "Invalid alignment");
			resourcePtrOut = pbaPlatParams->m_sharedStructuredBufferResources[writeIdx];
		}
		break;
		default: SEAssertF("Invalid PBDataType");
		}

		// Our heap offset is the base index of the stack we've allocated for each PBDataType
		heapOffsetOut = pbaPlatParams->AdvanceBaseIdx(pbDataType, util::CheckedCast<uint32_t>(alignedSize));
	}


	void ParameterBlockAllocator::Create(re::ParameterBlockAllocator& pba)
	{
		// Note: DX12 supports double or triple buffering. Currently we're using a hard-coded triple buffer, but we
		// don't need to. We clear the buffer we're writing to at the beginning of each new frame to ensure its
		// contents are no longer in use

		dx12::ParameterBlockAllocator::PlatformParams* pbaPlatformParams =
			pba.GetPlatformParams()->As<dx12::ParameterBlockAllocator::PlatformParams*>();

		pbaPlatformParams->m_sharedConstantBufferResources.resize(pbaPlatformParams->m_numBuffers, nullptr);
		pbaPlatformParams->m_sharedStructuredBufferResources.resize(pbaPlatformParams->m_numBuffers, nullptr);

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		SEAssert(re::ParameterBlockAllocator::k_fixedAllocationByteSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
			"Fixed allocation size must match the default resource placement alignment");

		const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC resourceDesc = 
			CD3DX12_RESOURCE_DESC::Buffer(re::ParameterBlockAllocator::k_fixedAllocationByteSize);

		for (uint8_t bufferIdx = 0; bufferIdx < pbaPlatformParams->m_numBuffers; bufferIdx++)
		{	
			HRESULT hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for constant buffers
				IID_PPV_ARGS(&pbaPlatformParams->m_sharedConstantBufferResources[bufferIdx]));
			CheckHResult(hr, "Failed to create committed resource");

			pbaPlatformParams->m_sharedConstantBufferResources[bufferIdx]->SetName(
				util::ToWideString(std::format("Shared constant buffer committed resource {}", bufferIdx)).c_str());

			hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for constant buffers
				IID_PPV_ARGS(&pbaPlatformParams->m_sharedStructuredBufferResources[bufferIdx]));
			CheckHResult(hr, "Failed to create committed resource");

			pbaPlatformParams->m_sharedStructuredBufferResources[bufferIdx]->SetName(
				util::ToWideString(std::format("Shared structured buffer committed resource {}", bufferIdx)).c_str());
		}
	}


	void ParameterBlockAllocator::Destroy(re::ParameterBlockAllocator& pba)
	{
		dx12::ParameterBlockAllocator::PlatformParams* pbaPlatformParams =
			pba.GetPlatformParams()->As<dx12::ParameterBlockAllocator::PlatformParams*>();

		SEAssert(pbaPlatformParams->m_sharedConstantBufferResources.size() == pbaPlatformParams->m_sharedStructuredBufferResources.size() &&
			pbaPlatformParams->m_numBuffers == pbaPlatformParams->m_sharedConstantBufferResources.size() &&
			pbaPlatformParams->m_numBuffers == dx12::RenderManager::GetNumFramesInFlight(),
			"Mismatched number of single frame buffers");

		pbaPlatformParams->m_sharedConstantBufferResources.assign(pbaPlatformParams->m_numBuffers, nullptr);
		pbaPlatformParams->m_sharedStructuredBufferResources.assign(pbaPlatformParams->m_numBuffers, nullptr);
	}
}