// © 2022 Adam Badke. All rights reserved.
#include "Buffer_DX12.h"
#include "BufferAllocator_DX12.h"
#include "BufferView.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "EnumTypes.h"
#include "EnumTypes_DX12.h"
#include "Fence_DX12.h"
#include "RenderManager.h"

#include "Core/Assert.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/MathUtils.h"

#include <d3dx12.h>

using Microsoft::WRL::ComPtr;


namespace
{
	constexpr uint32_t GetAlignment(re::BufferAllocator::AllocationPool allocationPool)
	{
		switch (allocationPool)
		{
		case re::BufferAllocator::Constant: return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256B
		case re::BufferAllocator::Structured: return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // 64KB
		case re::BufferAllocator::VertexStream: return 16; // Minimum alignment of a float4 is 16B
		case re::BufferAllocator::AllocationPool_Count:
		default:
			SEAssertF("Invalid buffer data type");
		}
		return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // This should never happen
	}


	inline uint64_t GetAlignedSize(re::Buffer::UsageMask usageMask, uint32_t bufferSize)
	{
		return util::RoundUpToNearestMultiple<uint64_t>(
			bufferSize,
			GetAlignment(re::BufferAllocator::BufferUsageMaskToAllocationPool(usageMask)));
	}


	inline D3D12_HEAP_TYPE MemoryPoolPreferenceToD3DHeapType(re::Buffer::MemoryPoolPreference memPoolPreference)
	{
		switch (memPoolPreference)
		{
		case::re::Buffer::DefaultHeap: return D3D12_HEAP_TYPE_DEFAULT;
		case::re::Buffer::UploadHeap: return D3D12_HEAP_TYPE_UPLOAD;
		default: SEAssertF("Invalid MemoryPoolPreference");
		}
		return D3D12_HEAP_TYPE_DEFAULT; // This should never happen
	}


	inline bool NeedsUAV(re::Buffer::BufferParams const& bufferParams)
	{
		return re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams.m_accessMask);
	}


	dx12::Buffer::ReadbackResource CreateReadbackResource(uint64_t bufferAlignedSize, wchar_t const* debugName)
	{
		dx12::Buffer::ReadbackResource readbackResource;

		const D3D12_HEAP_PROPERTIES readbackHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		const D3D12_RESOURCE_DESC readbackBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferAlignedSize);

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		constexpr D3D12_RESOURCE_STATES k_initialReadbackState = D3D12_RESOURCE_STATE_COPY_DEST;

		const HRESULT hr = device->CreateCommittedResource(
			&readbackHeapProperties,
			D3D12_HEAP_FLAG_NONE, // Zero our initial readback resource
			&readbackBufferDesc,
			k_initialReadbackState,
			nullptr,			// Optimized clear value: Null for buffers
			IID_PPV_ARGS(&readbackResource.m_resource));
		dx12::CheckHResult(hr, "Failed to create committed resource for readback buffer");

		readbackResource.m_resource->SetName(debugName);

		// Register the resource with the global resource state tracker:
		re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().RegisterResource(
			readbackResource.m_resource.Get(),
			k_initialReadbackState,
			1);

		return readbackResource;
	}
}


namespace dx12
{
	void Buffer::Create(re::Buffer& buffer)
	{
		re::Buffer::BufferParams const& bufferParams = buffer.GetBufferParams();
		
		SEAssert(!re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams) ||
			bufferParams.m_arraySize <= 1024, "Maximum offset of 1024 allowed into an SRV");

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		SEAssert(!params->m_isCreated, "Buffer is already created");
		params->m_isCreated = true;

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		const uint8_t numFramesInFlight = re::RenderManager::GetNumFramesInFlight();

		const uint32_t bufferSize = buffer.GetTotalBytes();
		const uint64_t alignedSize = GetAlignedSize(bufferParams.m_usageMask, bufferSize);

		constexpr D3D12_RESOURCE_STATES k_initialState = D3D12_RESOURCE_STATE_COMMON;

		const bool needsUAV = NeedsUAV(bufferParams);

		const D3D12_HEAP_TYPE outputBufferHeapType = MemoryPoolPreferenceToD3DHeapType(bufferParams.m_memPoolPreference);

		switch (buffer.GetLifetime())
		{
		case re::Lifetime::Permanent:
		{
			switch (buffer.GetAllocationType())
			{
			case re::Buffer::StagingPool::Permanent:
			{
				// We allocate N frames-worth of buffer space, and then set the m_heapByteOffset each frame
				const uint64_t allFramesAlignedSize = numFramesInFlight * alignedSize;

				const CD3DX12_HEAP_PROPERTIES heapProperties(outputBufferHeapType);
				const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(allFramesAlignedSize);

				HRESULT hr = device->CreateCommittedResource(
					&heapProperties,					// this heap will be used to upload the constant buffer data
					D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
					&resourceDesc,						// Size of the resource heap
					k_initialState,
					nullptr,							// Optimized clear value: None for constant buffers
					IID_PPV_ARGS(&params->m_resource));
				CheckHResult(hr, "Failed to create committed resource for mutable buffer");

				std::wstring const& debugName = buffer.GetWName() + L"_CPUMutable";
				params->m_resource->SetName(debugName.c_str());
			}
			break;
			case re::Buffer::StagingPool::Temporary:
			case re::Buffer::StagingPool::None:
			{
				// CPU-immutable buffers only need a single buffer's worth of space
				const CD3DX12_HEAP_PROPERTIES heapProperties(outputBufferHeapType);
				CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

				if (needsUAV)
				{
					resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				}

				HRESULT hr = device->CreateCommittedResource(
					&heapProperties,					// this heap will be used to upload the constant buffer data
					D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
					&resourceDesc,						// Size of the resource heap
					k_initialState,
					nullptr,							// Optimized clear value: None for constant buffers
					IID_PPV_ARGS(&params->m_resource));
				CheckHResult(hr, "Failed to create committed resource for immutable buffer");

				// Debug names:
				std::wstring const& debugName = buffer.GetWName() + L"_CPUImmutable";
				params->m_resource->SetName(debugName.c_str());
			}
			break;
			default: SEAssertF("Invalid AllocationType");
			}
		}
		break;
		case re::Lifetime::SingleFrame:
		{
			dx12::BufferAllocator* bufferAllocator =
				dynamic_cast<dx12::BufferAllocator*>(re::Context::Get()->GetBufferAllocator());

			bufferAllocator->GetSubAllocation(
				bufferParams.m_usageMask,
				alignedSize,
				params->m_heapByteOffset,
				params->m_resource);
		}
		break;
		default: SEAssertF("Invalid lifetime");
		}

		SEAssert(params->m_heapByteOffset % GetAlignment(
			re::BufferAllocator::BufferUsageMaskToAllocationPool(bufferParams.m_usageMask)) == 0,
			"Heap byte offset does not have the correct buffer alignment");
		
		// CPU readback:
		const bool cpuReadbackEnabled = re::Buffer::HasAccessBit(re::Buffer::CPURead, bufferParams);
		if (cpuReadbackEnabled)
		{
			for (uint8_t resourceIdx = 0; resourceIdx < numFramesInFlight; resourceIdx++)
			{
				std::wstring const& readbackDebugName = 
					buffer.GetWName() + L"_ReadbackBuffer" + std::to_wstring(resourceIdx);

				params->m_readbackResources.emplace_back(CreateReadbackResource(alignedSize, readbackDebugName.c_str()));
			}
		}

		// Register non-shared resources with the global resource state tracker:
		switch (buffer.GetLifetime())
		{
		case re::Lifetime::Permanent:
		{
			context->GetGlobalResourceStates().RegisterResource(params->m_resource.Get(), k_initialState, 1);
		}
		break;
		case re::Lifetime::SingleFrame:
		{
			//
		}
		break;
		default: SEAssertF("Invalid lifetime");
		}

#if defined(_DEBUG)
		void const* srcData = nullptr;
		uint32_t srcSize = 0;
		buffer.GetDataAndSize(&srcData, &srcSize);
		SEAssert(srcData == nullptr || srcSize <= alignedSize, "GetDataAndSize returned invalid results");
#endif
	}


	void Buffer::Update(
		re::Buffer const& buffer, uint8_t curFrameHeapOffsetFactor, uint32_t baseOffset, uint32_t numBytes)
	{
		re::Buffer::BufferParams const& bufferParams = buffer.GetBufferParams();

		SEAssert(re::Buffer::HasAccessBit(re::Buffer::CPUWrite, bufferParams) &&
			bufferParams.m_memPoolPreference == re::Buffer::UploadHeap,
			"CPU writes must be enabled to allow mapping");

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

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
		if (buffer.GetAllocationType() == re::Buffer::StagingPool::Permanent)
		{
			const uint64_t alignedSize = GetAlignedSize(bufferParams.m_usageMask, totalBytes);
			params->m_heapByteOffset = alignedSize * curFrameHeapOffsetFactor;
		}

		const bool updateAllBytes = baseOffset == 0 && (numBytes == 0 || numBytes == totalBytes);
		SEAssert(updateAllBytes || (baseOffset + numBytes <= totalBytes),
			"Base offset and number of bytes are out of bounds");

		// Adjust our pointers if we're doing a partial update:
		if (!updateAllBytes)
		{
			SEAssert(buffer.GetAllocationType() == re::Buffer::StagingPool::Permanent,
				"Only mutable buffers can be partially updated");

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
		uint32_t baseOffset,
		uint32_t numBytes,
		dx12::CommandList* copyCmdList,
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources)
	{
		dx12::Buffer::PlatformParams* params = buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		void const* data = buffer->GetData();

		data = static_cast<uint8_t const*>(data) + baseOffset;

		// Use the incoming numBytes rather than the buffer size: Might require a smaller buffer for partial updates
		const uint64_t alignedIntermediateBufferSize = GetAlignedSize(buffer->GetBufferParams().m_usageMask, numBytes);

		const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC intermediateBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedIntermediateBufferSize);

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
		memcpy(cpuVisibleData, data, numBytes);

		// Release the map:
		D3D12_RANGE writtenRange{
			0,
			numBytes };

		itermediateBufferResource->Unmap(
			k_intermediateSubresourceIdx,
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource

		// Schedule a copy from the intermediate resource to default/L1/vid memory heap via the copy queue:
		const uint32_t dstOffset = util::CheckedCast<uint32_t>(params->m_heapByteOffset);
		SEAssert(dstOffset == 0, "Immutable buffers always have m_heapByteOffset = 0, this is unexpected");

		copyCmdList->UpdateSubresources(buffer, dstOffset, itermediateBufferResource.Get(), 0, numBytes);

		// Released once the copy is done
		intermediateResources.emplace_back(itermediateBufferResource);
	}


	void Buffer::Destroy(re::Buffer& buffer)
	{
		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		SEAssert(params->m_isCreated, "Attempting to destroy a Buffer that has not been created");

		params->m_isCreated = false;

		SEAssert(params->m_resource != nullptr, "Resource pointer should not be null");

		// Unregister the resource from the global resource state tracker
		switch (buffer.GetLifetime())
		{
		case re::Lifetime::Permanent:
		{
			re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().UnregisterResource(params->m_resource.Get());
		}
		break;
		case re::Lifetime::SingleFrame:
		{
			//
		}
		break;
		default: SEAssertF("Invalid lifetime");
		}

		params->m_resource = nullptr;
		params->m_heapByteOffset = 0;
	}


	void const* Buffer::MapCPUReadback(re::Buffer const& buffer, uint8_t frameLatency)
	{
		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		re::RenderManager const* renderManager = re::RenderManager::Get();

		const uint32_t bufferSize = buffer.GetTotalBytes();

		// Compute the index of the readback resource we're mapping:
		SEAssert(renderManager->GetCurrentRenderFrameNum() >= frameLatency, "Frame latency would result in OOB access");
		const uint8_t readbackResourceIdx =
			(renderManager->GetCurrentRenderFrameNum() - frameLatency) % renderManager->GetNumFramesInFlight();

		// Ensure the GPU is finished with the buffer:
		{
			std::lock_guard<std::mutex> lock(params->m_readbackResources[readbackResourceIdx].m_readbackFenceMutex);

			const dx12::CommandListType resourceCopyCmdListType = dx12::Fence::GetCommandListTypeFromFenceValue(
				params->m_readbackResources[readbackResourceIdx].m_readbackFence);

			dx12::CommandQueue& resourceCopyQueue =
				re::Context::GetAs<dx12::Context*>()->GetCommandQueue(resourceCopyCmdListType);

			resourceCopyQueue.CPUWait(params->m_readbackResources[readbackResourceIdx].m_readbackFence);
		}

		const D3D12_RANGE readbackBufferRange{
			0,				// Begin
			bufferSize };	// End
		
		void* cpuVisibleData = nullptr;

		HRESULT hr = params->m_readbackResources[readbackResourceIdx].m_resource->Map(
			0,						// Subresource
			&readbackBufferRange,	// pReadRange
			&cpuVisibleData);		// ppData
		CheckHResult(hr, "Buffer::MapCPUReadback: Failed to map readback resource");

		params->m_currentMapFrameLatency = frameLatency;

		return cpuVisibleData;
	}


	void Buffer::UnmapCPUReadback(re::Buffer const& buffer)
	{
		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		re::RenderManager const* renderManager = re::RenderManager::Get();

		// Compute the index of the readback resource we're unmapping:
		SEAssert(renderManager->GetCurrentRenderFrameNum() >= params->m_currentMapFrameLatency,
			"Frame latency would result in OOB access");
		const uint8_t readbackResourceIdx =
			(renderManager->GetCurrentRenderFrameNum() - params->m_currentMapFrameLatency) % renderManager->GetNumFramesInFlight();


		const D3D12_RANGE writtenRange{
			0,		// Begin
			0 };	// End: Signifies CPU did not write any data when End <= Begin

		params->m_readbackResources[readbackResourceIdx].m_resource->Unmap(
			0,				// Subresource
			&writtenRange);	// pWrittenRange
	}


	D3D12_INDEX_BUFFER_VIEW const* dx12::Buffer::PlatformParams::GetOrCreateIndexBufferView(
		re::Buffer const& buffer, re::BufferView const& view)
	{
		SEAssert(!re::Buffer::HasUsageBit(re::Buffer::Usage::VertexStream, buffer) &&
			re::Buffer::HasUsageBit(re::Buffer::Usage::IndexStream, buffer),
			"Buffer does not have the correct usage flags set");

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		if (params->m_views.m_indexBufferView.BufferLocation == 0)
		{
			std::unique_lock<std::mutex> lock(params->m_viewMutex);

			if (params->m_views.m_indexBufferView.BufferLocation == 0)
			{
				params->m_views.m_indexBufferView = D3D12_INDEX_BUFFER_VIEW{
					.BufferLocation = params->m_resource->GetGPUVirtualAddress(),
					.SizeInBytes = buffer.GetTotalBytes(),
					.Format = dx12::DataTypeToDXGI_FORMAT(view.m_stream.m_dataType, false),
				};
			}
		}

		return &params->m_views.m_indexBufferView;
	}


	D3D12_VERTEX_BUFFER_VIEW const* dx12::Buffer::PlatformParams::GetOrCreateVertexBufferView(
		re::Buffer const& buffer, re::BufferView const& view)
	{
		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::VertexStream, buffer) &&
			!re::Buffer::HasUsageBit(re::Buffer::Usage::IndexStream, buffer),
			"Buffer does not have the correct usage flags set");

		SEAssert(view.m_stream.m_dataType != re::DataType::DataType_Count &&
			view.m_stream.m_dataType >= re::DataType::Float && 
			view.m_stream.m_dataType <= re::DataType::UByte4,
			"Invalid data type");

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		if (params->m_views.m_vertexBufferView.BufferLocation == 0)
		{
			std::unique_lock<std::mutex> lock(params->m_viewMutex);

			if (params->m_views.m_vertexBufferView.BufferLocation == 0)
			{
				params->m_views.m_vertexBufferView = D3D12_VERTEX_BUFFER_VIEW{
					.BufferLocation = params->m_resource->GetGPUVirtualAddress(),
					.SizeInBytes = buffer.GetTotalBytes(),
					.StrideInBytes = DataTypeToStride(view.m_stream.m_dataType),
				};
			}
		}

		return &params->m_views.m_vertexBufferView;
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetSRV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatformParams const* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		return bufferPlatParams->m_srvDescriptors.GetCreateDescriptor(buffer, view);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetUAV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatformParams const* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		return bufferPlatParams->m_uavDescriptors.GetCreateDescriptor(buffer, view);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetCBV(re::Buffer const* buffer, re::BufferView const& view)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatformParams const* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		return bufferPlatParams->m_cbvDescriptors.GetCreateDescriptor(buffer, view);
	}
}