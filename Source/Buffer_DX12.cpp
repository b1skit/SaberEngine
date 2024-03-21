// � 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "CastUtils.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "MathUtils.h"
#include "Buffer_DX12.h"
#include "BufferAllocator_DX12.h"
#include "RenderManager.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

using Microsoft::WRL::ComPtr;


namespace
{
	constexpr uint32_t GetAlignment(re::Buffer::DataType dataType)
	{
		switch (dataType)
		{
		case re::Buffer::DataType::Constant:
		{
			return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // We must allocate CBVs in multiples of 256B
		}
		break;
		case re::Buffer::DataType::Structured:
		{
			return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // We must allocate SRVs in multiples of 64KB
		}
		break;
		case re::Buffer::DataType::DataType_Count:
		default:
			SEAssertF("Invalid buffer data type");
		}
		return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	}


	uint64_t GetAlignedSize(re::Buffer::DataType dataType, uint32_t bufferSize)
	{
		return util::RoundUpToNearestMultiple<uint64_t>(bufferSize, GetAlignment(dataType));
	}


	D3D12_HEAP_TYPE GetHeapTypeFromBufferUsage(uint8_t usageMask)
	{
		if (usageMask & re::Buffer::Usage::CPUWrite)
		{
			return D3D12_HEAP_TYPE_UPLOAD;
		}
		else if (usageMask & re::Buffer::Usage::CPURead)
		{
			return D3D12_HEAP_TYPE_READBACK;
		}
		return D3D12_HEAP_TYPE_DEFAULT;
	}


	bool NeedsUAV(re::Buffer::BufferParams const& bufferParams)
	{
		return bufferParams.m_type == re::Buffer::Type::Immutable &&
			(bufferParams.m_usageMask & re::Buffer::Usage::GPUWrite);
	}
}


namespace dx12
{
	void Buffer::Create(re::Buffer& buffer)
	{
		SEAssert(buffer.GetBufferParams().m_dataType != re::Buffer::DataType::Structured ||
			buffer.GetBufferParams().m_numElements <= 1024, "Maximum offset of 1024 allowed into an SRV");

		dx12::Buffer::PlatformParams* params =
			buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		SEAssert(!params->m_isCreated, "Buffer is already created");
		params->m_isCreated = true;

		const uint32_t bufferSize = buffer.GetSize();
		const uint64_t alignedSize = GetAlignedSize(buffer.GetBufferParams().m_dataType, bufferSize);

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		const uint8_t numFramesInFlight = re::RenderManager::GetNumFramesInFlight();

		// Note: Our buffers live in the upload heap, as they're typically small and updated frequently. 
		// No point copying them to VRAM, for now

		const D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON;
		const D3D12_HEAP_TYPE heapType = GetHeapTypeFromBufferUsage(buffer.GetBufferParams().m_usageMask);

		const bool needsUAV = NeedsUAV(buffer.GetBufferParams());

		switch (buffer.GetType())
		{
		case re::Buffer::Type::Mutable:
		{
			// We allocate N frames-worth of buffer space, and then set the m_heapByteOffset each frame
			const uint64_t allFramesAlignedSize = numFramesInFlight * alignedSize;

			const CD3DX12_HEAP_PROPERTIES heapProperties(heapType);
			const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(allFramesAlignedSize);
			
			HRESULT hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				//&bufferDesc,
				initialResourceState,
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
			const CD3DX12_HEAP_PROPERTIES heapProperties(heapType);
			CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

			if (needsUAV)
			{
				resourceDesc.Flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			HRESULT hr = device->CreateCommittedResource(
				&heapProperties,					// this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
				&resourceDesc,						// Size of the resource heap
				initialResourceState,
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
			dx12::BufferAllocator* bufferAllocator = 
				dynamic_cast<dx12::BufferAllocator*>(re::Context::Get()->GetBufferAllocator());
			
			bufferAllocator->GetSubAllocation(
				buffer.GetBufferParams().m_dataType,
				alignedSize,
				params->m_heapByteOffset,
				params->m_resource);
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		const uint32_t alignment = GetAlignment(buffer.GetBufferParams().m_dataType);

		SEAssert(params->m_heapByteOffset % alignment == 0,
			"Heap byte offset does not have the correct buffer alignment");
		
		// Create the appropriate resource views:
		// Note: We (currently) exclusively set Buffer CBVs & SRVs inline directly in the root signature
		if (needsUAV)
		{
			// Register the resource with the global resource state tracker:
			context->GetGlobalResourceStates().RegisterResource(
				params->m_resource.Get(),
				initialResourceState,
				1);

			params->m_uavCPUDescAllocation = std::move(context->GetCPUDescriptorHeapMgr(
				CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV).Allocate(1));

			const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc
			{
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
				.Buffer = D3D12_BUFFER_UAV
					{
					// Offset the view within the entire N-frames of resource data
					.FirstElement = 0,
					.NumElements = buffer.GetBufferParams().m_numElements,
					.StructureByteStride = buffer.GetStride(), // Size of the struct in the shader
					.CounterOffsetInBytes = 0,
					.Flags = D3D12_BUFFER_UAV_FLAG_NONE,
				}
			};
			device->CreateUnorderedAccessView(
				params->m_resource.Get(),
				nullptr, // Optional counter resource
				&uavDesc,
				params->m_uavCPUDescAllocation.GetBaseDescriptor());
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
		SEAssert((buffer.GetBufferParams().m_usageMask & re::Buffer::Usage::CPUWrite) != 0,
			"CPU writes must be enabled to allow mapping");

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


	void Buffer::Update(
		re::Buffer const* buffer,
		dx12::CommandList* copyCmdList,
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources)
	{
		SEAssert((buffer->GetBufferParams().m_usageMask & re::Buffer::CPUWrite) == 0,
			"Buffers with CPU writes enabled should be updated by a mapped pointer");

		dx12::Buffer::PlatformParams* params =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		void const* data = nullptr;
		uint32_t totalBytes = 0;
		buffer->GetDataAndSize(&data, &totalBytes);

		const uint64_t alignedSize = GetAlignedSize(buffer->GetBufferParams().m_dataType, totalBytes);

		// We might require a smaller intermediate buffer if we're only doing a partial update
		const uint64_t totalAlignedIntermediateBufferSize = 
			GetAlignedSize(buffer->GetBufferParams().m_dataType, totalBytes);

		// Create an intermediate buffer staging buffer:
		const uint32_t intermediateBufferWidth = util::RoundUpToNearestMultiple(
			util::CheckedCast<uint32_t>(totalAlignedIntermediateBufferSize),
			static_cast<uint32_t>(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));


		const D3D12_RESOURCE_DESC intermediateBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(intermediateBufferWidth);
		const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		ComPtr<ID3D12Resource> itermediateBufferResource = nullptr;

		HRESULT hr = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
			&intermediateBufferResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&itermediateBufferResource));
		CheckHResult(hr, "Failed to create intermediate texture buffer resource");

		std::wstring const& intermediateName = buffer->GetWName() + L" intermediate buffer";
		itermediateBufferResource->SetName(intermediateName.c_str());

		constexpr uint32_t k_intermediateSubresourceIdx = 0;

		// Map the intermediate resource, and copy our data into it
		void* cpuVisibleData = nullptr;
		const CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU (end <= begin)
		hr = itermediateBufferResource->Map(
			k_intermediateSubresourceIdx,
			&readRange,
			&cpuVisibleData);
		CheckHResult(hr, "Buffer::Update: Failed to map intermediate committed resource");

		// Copy our data to the appropriate offset in the cpu-visible heap:
		memcpy(cpuVisibleData, data, totalBytes);

		// Release the map:
		D3D12_RANGE writtenRange{
			0,
			totalBytes };

		itermediateBufferResource->Unmap(
			k_intermediateSubresourceIdx,
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource

		// Schedule a copy from the intermediate resource to default/L1/vid memory heap via the copy queue:
		const uint32_t dstOffset = util::CheckedCast<uint32_t>(params->m_heapByteOffset);
		SEAssert(dstOffset == 0, "Immutable buffers always have m_heapByteOffset = 0, this is unexpected");

		copyCmdList->UpdateSubresources(buffer, dstOffset, itermediateBufferResource.Get(), 0, totalBytes);

		// Released once the copy is done
		intermediateResources.emplace_back(itermediateBufferResource);
	}


	void Buffer::Destroy(re::Buffer& buffer)
	{
		dx12::Buffer::PlatformParams* params =
			buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		SEAssert(params->m_isCreated, "Attempting to destroy a Buffer that has not been created");

		params->m_isCreated = false;

		SEAssert(params->m_resource != nullptr, "Resource pointer should not be null");

		if (NeedsUAV(buffer.GetBufferParams()))
		{
			// Unregister the resource from the global resource state tracker
			re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().UnregisterResource(params->m_resource.Get());
		}
		params->m_resource = nullptr;
		params->m_heapByteOffset = 0;
		params->m_uavCPUDescAllocation.Free(0);
	}
}