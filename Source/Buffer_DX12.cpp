// © 2022 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "Context_DX12.h"
#include "Assert.h"
#include "Debug_DX12.h"
#include "MathUtils.h"
#include "Buffer_DX12.h"
#include "BufferAllocator_DX12.h"
#include "RenderManager.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

using Microsoft::WRL::ComPtr;


namespace
{
	uint64_t GetAlignedSize(re::Buffer::DataType dataType, uint32_t bufferSize)
	{
		uint64_t alignedSize = 0;
		switch (dataType)
		{
		case re::Buffer::DataType::Constant:
		{
			// We must allocate CBVs in multiples of 256B			
			alignedSize = util::RoundUpToNearestMultiple<uint64_t>(bufferSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}
		break;
		case re::Buffer::DataType::Structured:
		{
			// We must allocate SRVs in multiples of 64KB
			alignedSize = util::RoundUpToNearestMultiple<uint64_t>(bufferSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		}
		break;
		case re::Buffer::DataType::DataType_Count:
		default:
			SEAssertF("Invalid buffer data type");
		}
		
		return alignedSize;
	}
}

namespace dx12
{
	void Buffer::Create(re::Buffer& buffer)
	{
		dx12::Buffer::PlatformParams* params =
			buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		SEAssert(!params->m_isCreated, "Buffer is already created");
		params->m_isCreated = true;

		const uint32_t bufferSize = buffer.GetSize();
		const uint64_t alignedSize = GetAlignedSize(buffer.GetBufferParams().m_dataType, bufferSize);

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		// Note: Our buffers live in the upload heap, as they're typically small and updated frequently. 
		// No point copying them to VRAM, for now

		switch (buffer.GetType())
		{
		case re::Buffer::Type::Mutable:
		{
			// We allocate N frames-worth of buffer space, and then set the m_heapByteOffset each frame
			const uint8_t numFramesInFlight = re::RenderManager::GetNumFramesInFlight();
			const uint64_t allFramesAlignedSize = numFramesInFlight * alignedSize;

			const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
			const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(allFramesAlignedSize);

			HRESULT hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				D3D12_RESOURCE_STATE_GENERIC_READ,	// Mandatory for D3D12_HEAP_TYPE_UPLOAD heaps
				nullptr,							// Optimized clear value: None for constant buffers
				IID_PPV_ARGS(&params->m_resource));
			CheckHResult(hr, "Failed to create committed resource");

			std::wstring const& debugName = buffer.GetWName() + L"_Mutable";
			params->m_resource->SetName(debugName.c_str());
		}
		break;
		case re::Buffer::Type::Immutable:
		{
			// Immutable buffers cannot change frame-to-frame, thus only need a single buffer's worth of space
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
			std::wstring const& debugName = buffer.GetWName() + L"_Immutable";;
			params->m_resource->SetName(debugName.c_str());
		}
		break;
		case re::Buffer::Type::SingleFrame:
		{
			dx12::BufferAllocator::GetSubAllocation(
				buffer.GetBufferParams().m_dataType,
				alignedSize,
				params->m_heapByteOffset,
				params->m_resource);
		}
		break;
		default: SEAssertF("Invalid Type");
		}
		
		// Create the appropriate resource view:
		// Note: We (currently) exclusively set Buffer SRVs inline directly in the root signature
		switch (buffer.GetBufferParams().m_dataType)
		{
		case re::Buffer::DataType::Constant:
		{
			SEAssert(params->m_heapByteOffset % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
				"CBV buffer offsets must be multiples of D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT");

			SEAssert(buffer.GetBufferParams().m_numElements == 1, "Constant buffers only support a single element. Arrays are achieved "
				"as a member variable within a single constant buffer");

			// Allocate a cpu-visible descriptor to hold our view:
			params->m_srvCPUDescAllocation = std::move(re::Context::GetAs<dx12::Context*>()->GetCPUDescriptorHeapMgr(
				CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(buffer.GetBufferParams().m_numElements));

			// Create a constant buffer view:
			const D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc
			{
				.BufferLocation = params->m_resource->GetGPUVirtualAddress() + params->m_heapByteOffset, // Multiples of D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
				.SizeInBytes = util::CheckedCast<uint32_t>(alignedSize) // Must be padded/aligned size
			}; 

			device->CreateConstantBufferView(
				&constantBufferViewDesc,
				params->m_srvCPUDescAllocation.GetBaseDescriptor());
		}
		break;
		case re::Buffer::DataType::Structured:
		{
			SEAssert(buffer.GetSize() % buffer.GetBufferParams().m_numElements == 0,
				"Size must be equally divisible by the number of elements");
			SEAssert(buffer.GetBufferParams().m_numElements <= 1024, "Maximum offset of 1024 allowed into an SRV");
			SEAssert(params->m_heapByteOffset % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 0,
				"CBV buffer offsets must be multiples of D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT");

			constexpr uint32_t k_numDescriptors = 1; // Only need a single descriptor to represent an array of elements
			params->m_srvCPUDescAllocation = std::move(re::Context::GetAs<dx12::Context*>()->GetCPUDescriptorHeapMgr(
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
					.NumElements = buffer.GetBufferParams().m_numElements,
					.StructureByteStride = buffer.GetStride(), // Size of the struct in the shader
					.Flags = D3D12_BUFFER_SRV_FLAGS::D3D12_BUFFER_SRV_FLAG_NONE }};

			device->CreateShaderResourceView(
				params->m_resource.Get(),
				&srvDesc,
				params->m_srvCPUDescAllocation.GetBaseDescriptor());
		}
		break;
		case re::Buffer::DataType::DataType_Count:
		default:
			SEAssertF("Invalid buffer data type");
		}

#if defined(_DEBUG)
		void const* srcData = nullptr;
		uint32_t srcSize = 0;
		buffer.GetDataAndSize(&srcData, &srcSize);
		SEAssert(srcData != nullptr && srcSize <= alignedSize, "GetDataAndSize returned invalid results");
#endif
	}


	void Buffer::Update(
		re::Buffer const& buffer, uint8_t curFrameHeapOffsetFactor, uint32_t baseOffset, uint32_t numBytes)
	{
		dx12::Buffer::PlatformParams* params =
			buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		constexpr uint32_t k_subresourceIdx = 0;

		// Get a CPU pointer to the subresource (i.e subresource 0)
		void* cpuVisibleData = nullptr;
		const CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU (end <= begin)
		HRESULT hr = params->m_resource->Map(
			k_subresourceIdx,
			&readRange,
			&cpuVisibleData);
		CheckHResult(hr, "Buffer::Update: Failed to map committed resource");

		// We map and then unmap immediately; Microsoft recommends resources be left unmapped while the CPU will not 
		// modify them, and use tight, accurate ranges at all times
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map
		void const* data = nullptr;
		uint32_t totalBytes = 0;
		buffer.GetDataAndSize(&data, &totalBytes);

		// Update the heap offset, if required
		if (buffer.GetType() == re::Buffer::Type::Mutable)
		{
			const uint64_t alignedSize = GetAlignedSize(buffer.GetBufferParams().m_dataType, totalBytes);
			params->m_heapByteOffset = alignedSize * curFrameHeapOffsetFactor;
		}

		const bool updateAllBytes = baseOffset == 0 && (numBytes == 0 || numBytes == totalBytes);
		SEAssert(updateAllBytes || (baseOffset + numBytes <= totalBytes),
			"Base offset and number of bytes are out of bounds");

		// Adjust our pointers if we're doing a partial update:
		if (!updateAllBytes)
		{
			SEAssert(buffer.GetType() == re::Buffer::Type::Mutable, "Only mutable buffers can be partially updated");

			// Update the source data pointer:
			data = static_cast<uint8_t const*>(data) + baseOffset;
			totalBytes = numBytes;

			// Update the destination pointer:
			cpuVisibleData = static_cast<uint8_t*>(cpuVisibleData) + baseOffset;
		}

		// Copy our data to the appropriate offset in the cpu-visible heap:
		void* offsetPtr = static_cast<uint8_t*>(cpuVisibleData) + params->m_heapByteOffset;
		memcpy(offsetPtr, data, totalBytes);
	
		// Release the map:
		D3D12_RANGE writtenRange{
			params->m_heapByteOffset + baseOffset,
			params->m_heapByteOffset + baseOffset + totalBytes };

		params->m_resource->Unmap(
			k_subresourceIdx,	// Subresource
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource
	}


	void Buffer::Destroy(re::Buffer& buffer)
	{
		dx12::Buffer::PlatformParams* params =
			buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		SEAssert(params->m_isCreated, "Attempting to destroy a Buffer that has not been created");

		params->m_isCreated = false;

		params->m_resource = nullptr;
		params->m_heapByteOffset = 0;

		params->m_srvCPUDescAllocation.Free(0);
	}
}