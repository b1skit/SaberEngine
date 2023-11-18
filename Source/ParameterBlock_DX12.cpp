// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "CastUtils.h"
#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "MathUtils.h"
#include "ParameterBlock_DX12.h"
#include "ParameterBlockAllocator_DX12.h"
#include "RenderManager.h"


namespace dx12
{
	void ParameterBlock::Create(re::ParameterBlock& paramBlock)
	{
		dx12::ParameterBlock::PlatformParams* params =
			paramBlock.GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();
		SEAssert("Parameter block is already created", !params->m_isCreated);
		params->m_isCreated = true;

		const uint32_t pbSize = paramBlock.GetSize();
		uint64_t alignedSize = 0;
		switch(params->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			// We must allocate CBVs in multiples of 256B			
			alignedSize = util::RoundUpToNearestMultiple<uint64_t>(pbSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			// We must allocate SRVs in multiples of 64KB
			alignedSize = util::RoundUpToNearestMultiple<uint64_t>(pbSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		}
		break;
		case re::ParameterBlock::PBDataType::PBDataType_Count:
		default:
			SEAssertF("Invalid parameter block data type");
		}


		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		const re::ParameterBlock::PBType pbType = paramBlock.GetType();
		switch (pbType)
		{
		case re::ParameterBlock::PBType::Mutable:
		case re::ParameterBlock::PBType::Immutable:
		{
			// Our parameter blocks live in the upload heap, as they're typically small and updated frequently. 
			// No point copying them to VRAM, for now
			const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
			const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

			HRESULT hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for constant buffers
				IID_PPV_ARGS(&params->m_resource));
			CheckHResult(hr, "Failed to create committed resource");

			// Debug names:
			std::wstring debugName = paramBlock.GetWName();
			switch (paramBlock.GetType())
			{
			case re::ParameterBlock::PBType::Mutable:
			{
				debugName += L"_Mutable";
			}
			break;
			case re::ParameterBlock::PBType::Immutable:
			{
				debugName += L"_Immutable";
			}
			break;
			case re::ParameterBlock::PBType::SingleFrame:
			{
				debugName += L"_SingleFrame#" + std::to_wstring(re::RenderManager::Get()->GetCurrentRenderFrameNum());
			}
			break;
			default:
				SEAssertF("Invalid parameter block type");
			}
			params->m_resource->SetName(debugName.c_str());
		}
		break;
		case re::ParameterBlock::PBType::SingleFrame:
		{
			dx12::ParameterBlockAllocator::GetSubAllocation(
				params->m_dataType,
				alignedSize,
				params->m_heapByteOffset,
				params->m_resource);
		}
		break;
		default: SEAssertF("Invalid PBType");
		}
		
		// Note: We (currently) exclusively set ParameterBlocks inline directly in the root signature, so these views
		// never actually get used anywhere yet. TODO: Support this

		// Create the appropriate resource view:
		switch (params->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			SEAssert("CBV buffer offsets must be multiples of D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT", 
				params->m_heapByteOffset % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);

			// NOTE: dx12::CommandList::SetParameterBlock will need to be updated when we solve the PB CBV/SRV issue
			SEAssert("TODO: Handle arrays of CBVs", params->m_numElements == 1);

			// Allocate a cpu-visible descriptor to hold our view:
			params->m_cpuDescAllocation = std::move(re::Context::GetAs<dx12::Context*>()->GetCPUDescriptorHeapMgr(
				CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(params->m_numElements));

			// Create a constant buffer view:
			const D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
				.BufferLocation = params->m_resource->GetGPUVirtualAddress() + params->m_heapByteOffset, // Multiples of D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
				.SizeInBytes = util::CheckedCast<uint32_t>(alignedSize)}; // Must be padded/aligned size

			device->CreateConstantBufferView(
				&constantBufferViewDesc,
				params->m_cpuDescAllocation.GetBaseDescriptor());
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			SEAssert("Size must be equally divisible by the number of elements",
				paramBlock.GetSize() % params->m_numElements == 0);
			SEAssert("Maximum offset of 1024 allowed into an SRV", params->m_numElements <= 1024);
			SEAssert("CBV buffer offsets must be multiples of D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT",
				params->m_heapByteOffset % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0);

			constexpr uint32_t k_numDescriptors = 1; // Only need a single descriptor to represent an array of elements
			params->m_cpuDescAllocation = std::move(re::Context::GetAs<dx12::Context*>()->GetCPUDescriptorHeapMgr(
				CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(k_numDescriptors));

			// .FirstElement is the index of the first element to be accessed by the view:
			const uint32_t firstElementOffset = 
				util::CheckedCast<uint32_t>(params->m_heapByteOffset / D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

			// Create an SRV:
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = D3D12_SHADER_RESOURCE_VIEW_DESC{
				.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = D3D12_BUFFER_SRV{
					.FirstElement = firstElementOffset,
					.NumElements = params->m_numElements,
					.StructureByteStride = paramBlock.GetStride(), // Size of the struct in the shader
					.Flags = D3D12_BUFFER_SRV_FLAGS::D3D12_BUFFER_SRV_FLAG_NONE }};

			device->CreateShaderResourceView(
				params->m_resource.Get(),
				&srvDesc,
				params->m_cpuDescAllocation.GetBaseDescriptor());
		}
		break;
		case re::ParameterBlock::PBDataType::PBDataType_Count:
		default:
			SEAssertF("Invalid parameter block data type");
		}

#if defined(_DEBUG)
		void const* srcData = nullptr;
		uint32_t srcSize = 0;
		paramBlock.GetDataAndSize(srcData, srcSize);
		SEAssert("GetDataAndSize returned invalid results", srcData != nullptr && srcSize <= alignedSize);
#endif
	}


	void ParameterBlock::Update(re::ParameterBlock& paramBlock)
	{
		dx12::ParameterBlock::PlatformParams* params =
			paramBlock.GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();

		constexpr uint32_t k_subresourceIdx = 0;

		// Get a CPU pointer to the subresource (i.e subresource 0)
		void* cpuVisibleData = nullptr;
		CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU (end <= begin)
		HRESULT hr = params->m_resource->Map(
			k_subresourceIdx,
			&readRange,
			&cpuVisibleData);
		CheckHResult(hr, "ParameterBlock::Update: Failed to map committed resource");

		// We map and then unmap immediately; Microsoft recommends resources be left unmapped while the CPU will not 
		// modify them, and use tight,accurate ranges at all times
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map
		void const* srcData = nullptr;
		uint32_t srcSize = 0;
		paramBlock.GetDataAndSize(srcData, srcSize);

		// Copy our data to the appropriate offset in the cpu-visible heap:
		void* offsetPtr = static_cast<uint8_t*>(cpuVisibleData) + params->m_heapByteOffset;
		memcpy(offsetPtr, srcData, srcSize);

		// Release the map
		D3D12_RANGE writtenRange{ 
			params->m_heapByteOffset, 
			params->m_heapByteOffset + srcSize };

		params->m_resource->Unmap(
			k_subresourceIdx,	// Subresource
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource
	}


	void ParameterBlock::Destroy(re::ParameterBlock& paramBlock)
	{
		dx12::ParameterBlock::PlatformParams* params =
			paramBlock.GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();
		SEAssert("Attempting to destroy a ParameterBlock that has not been created", params->m_isCreated);

		params->m_dataType = re::ParameterBlock::PBDataType::PBDataType_Count;
		params->m_numElements = 0;
		params->m_isCreated = false;

		params->m_resource = nullptr;
		params->m_heapByteOffset = 0;

		params->m_cpuDescAllocation.Free(0);
	}
}