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
		case re::BufferAllocator::Raw: return 16; // Minimum alignment of a float4 is 16B
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


	dx12::Buffer::ReadbackResource CreateReadbackResource(uint64_t numBytes, wchar_t const* debugName)
	{
		dx12::Buffer::ReadbackResource readbackResource;

		dx12::HeapManager& heapMgr = re::Context::GetAs<dx12::Context*>()->GetHeapManager();

		readbackResource.m_readbackGPUResource = heapMgr.CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(numBytes),
				.m_heapType = D3D12_HEAP_TYPE_READBACK,
				.m_initialState = D3D12_RESOURCE_STATE_COPY_DEST,
			},
			debugName);

		return readbackResource;
	}


	std::wstring CreateDebugName(re::Buffer const& buffer)
	{
		switch (buffer.GetLifetime())
		{
		case re::Lifetime::Permanent:
		{
			switch (buffer.GetStagingPool())
			{
			case re::Buffer::StagingPool::Permanent:
			{
				return buffer.GetWName() + L"_CPUMutable";
			}
			break;
			case re::Buffer::StagingPool::Temporary:
			case re::Buffer::StagingPool::None:
			{
				return buffer.GetWName() + L"_CPUImmutable";
			}
			break;
			default: SEAssertF("Invalid AllocationType");
			}
		}
		break;
		case re::Lifetime::SingleFrame:
		{
			return buffer.GetWName() + L"_SingleFrame";
		}
		break;
		default: SEAssertF("Invalid lifetime");
		}
		return L"CreateDebugName failed"; // This should never happen
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

		const uint8_t numFramesInFlight = re::RenderManager::GetNumFramesInFlight();

		const re::Lifetime bufferLifetime = buffer.GetLifetime();

		const bool needsUAV = NeedsUAV(bufferParams);

		uint32_t requestedSize = buffer.GetTotalBytes();
		if (bufferLifetime == re::Lifetime::Permanent &&
			buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent)
		{
			// We allocate N aligned frames-worth of buffer space, and then set the m_heapByteOffset each frame
			requestedSize = util::CheckedCast<uint32_t>(
				GetAlignedSize(bufferParams.m_usageMask, requestedSize) * numFramesInFlight);
		}

		// Single frame buffers sub-allocated from a single resource:
		if (bufferLifetime == re::Lifetime::SingleFrame &&
			bufferParams.m_memPoolPreference == re::Buffer::MemoryPoolPreference::UploadHeap &&
			!needsUAV)
		{
			dx12::BufferAllocator* bufferAllocator =
				dynamic_cast<dx12::BufferAllocator*>(re::Context::Get()->GetBufferAllocator());

			bufferAllocator->GetSubAllocation(
				bufferParams.m_usageMask,
				GetAlignedSize(bufferParams.m_usageMask, requestedSize),
				params->m_heapByteOffset,
				params->m_resolvedGPUResource);

			SEAssert(params->m_heapByteOffset % GetAlignment(
				re::BufferAllocator::BufferUsageMaskToAllocationPool(bufferParams.m_usageMask)) == 0,
				"Heap byte offset does not have the correct buffer alignment");
		}
		else // Placed resources via the heap manager:
		{
			CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(requestedSize);
			if (needsUAV)
			{
				bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			std::wstring const& debugName = CreateDebugName(buffer);

			params->m_gpuResource = re::Context::GetAs<dx12::Context*>()->GetHeapManager().CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = bufferDesc,
					.m_heapType = MemoryPoolPreferenceToD3DHeapType(bufferParams.m_memPoolPreference),
					.m_initialState = D3D12_RESOURCE_STATE_COMMON,
				},
				debugName.c_str());

			params->m_resolvedGPUResource = params->m_gpuResource->Get();
		}
		
		// CPU readback:
		const bool cpuReadbackEnabled = re::Buffer::HasAccessBit(re::Buffer::CPURead, bufferParams);
		if (cpuReadbackEnabled)
		{
			for (uint8_t resourceIdx = 0; resourceIdx < numFramesInFlight; resourceIdx++)
			{
				std::wstring const& readbackDebugName = 
					buffer.GetWName() + L"_ReadbackBuffer" + std::to_wstring(resourceIdx);

				params->m_readbackResources.emplace_back(
					CreateReadbackResource(buffer.GetTotalBytes(), readbackDebugName.c_str()));
			}
		}
	}


	void Buffer::Update(
		re::Buffer const& buffer, uint8_t curFrameHeapOffsetFactor, uint32_t baseOffset, uint32_t numBytes)
	{
		re::Buffer::BufferParams const& bufferParams = buffer.GetBufferParams();

		SEAssert(re::Buffer::HasAccessBit(re::Buffer::CPUWrite, bufferParams) &&
			bufferParams.m_memPoolPreference == re::Buffer::UploadHeap,
			"CPU writes must be enabled to allow mapping");

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		// Get a CPU pointer to the subresource (i.e subresource 0)
		void* cpuVisibleData = nullptr;
		const CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU (end <= begin)
		HRESULT hr = params->m_resolvedGPUResource->Map(
			0,									// Subresource index: Buffers only have a single subresource
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
		if (buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent)
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
			SEAssert(buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent,
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
		const D3D12_RANGE writtenRange{
			params->m_heapByteOffset + baseOffset,
			params->m_heapByteOffset + baseOffset + totalBytes };

		params->m_resolvedGPUResource->Unmap(
			0,					// Subresource index: Buffers only have a single subresource
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource
	}


	void Buffer::Update(
		re::Buffer const* buffer,
		uint32_t baseOffset,
		uint32_t numBytes,
		dx12::CommandList* copyCmdList)
	{
		dx12::HeapManager& heapMgr = re::Context::GetAs<dx12::Context*>()->GetHeapManager();

		dx12::Buffer::PlatformParams* params = buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		void const* data = buffer->GetData();

		data = static_cast<uint8_t const*>(data) + baseOffset;

		// Use the incoming numBytes rather than the buffer size: Might require a smaller buffer for partial updates
		const uint64_t alignedIntermediateBufferSize = GetAlignedSize(buffer->GetBufferParams().m_usageMask, numBytes);
		
		// GPUResources automatically use a deferred deletion, it is safe to let this go out of scope immediately
		std::wstring const& intermediateName = buffer->GetWName() + L" intermediate GPU buffer resource";
		std::unique_ptr<dx12::GPUResource> intermediateResource = heapMgr.CreateResource(dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedIntermediateBufferSize),
				.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
				.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ,
			},
			intermediateName.c_str());

		constexpr uint32_t k_intermediateSubresourceIdx = 0;

		// Map the intermediate resource, and copy our data into it
		void* cpuVisibleData = nullptr;
		const CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU (end <= begin)
		const HRESULT hr = intermediateResource->Map(
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

		intermediateResource->Unmap(
			k_intermediateSubresourceIdx,
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource

		// Schedule a copy from the intermediate resource to default/L1/vid memory heap via the copy queue:
		copyCmdList->UpdateSubresources(buffer, baseOffset, intermediateResource->Get(), 0, numBytes);
	}


	void Buffer::Destroy(re::Buffer& buffer)
	{
		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
		SEAssert(params->m_isCreated, "Attempting to destroy a Buffer that has not been created");

		params->m_isCreated = false;

		SEAssert((params->m_gpuResource && params->m_gpuResource->IsValid()) || 
			(!params->m_gpuResource && params->m_resolvedGPUResource != nullptr),
			"GPUResource should be valid");

		params->m_gpuResource = nullptr;
		params->m_resolvedGPUResource = nullptr;
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

		HRESULT hr = params->m_readbackResources[readbackResourceIdx].m_readbackGPUResource->Map(
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

		params->m_readbackResources[readbackResourceIdx].m_readbackGPUResource->Unmap(
			0,				// Subresource
			&writtenRange);	// pWrittenRange
	}


	D3D12_INDEX_BUFFER_VIEW const* dx12::Buffer::GetOrCreateIndexBufferView(
		re::Buffer const& buffer, re::BufferView const& view)
	{
		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer),
			"Buffer does not have the correct usage flags set");

		dx12::Buffer::PlatformParams* params = buffer.GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		if (params->m_views.m_indexBufferView.BufferLocation == 0)
		{
			std::unique_lock<std::mutex> lock(params->m_viewMutex);

			if (params->m_views.m_indexBufferView.BufferLocation == 0)
			{
				params->m_views.m_indexBufferView = D3D12_INDEX_BUFFER_VIEW{
					.BufferLocation = params->m_resolvedGPUResource->GetGPUVirtualAddress() + params->m_heapByteOffset,
					.SizeInBytes = buffer.GetTotalBytes(),
					.Format = dx12::DataTypeToDXGI_FORMAT(view.m_stream.m_dataType, false),
				};
			}
		}

		return &params->m_views.m_indexBufferView;
	}


	D3D12_VERTEX_BUFFER_VIEW const* dx12::Buffer::GetOrCreateVertexBufferView(
		re::Buffer const& buffer, re::BufferView const& view)
	{
		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer),
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
					.BufferLocation = params->m_resolvedGPUResource->GetGPUVirtualAddress() + params->m_heapByteOffset,
					.SizeInBytes = buffer.GetTotalBytes(),
					.StrideInBytes = DataTypeToByteStride(view.m_stream.m_dataType),
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


	D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetGPUVirtualAddress(re::Buffer const* buffer)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatformParams const* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		// Apply the heap byte offset to account for sub-allocated Buffers 
		return bufferPlatParams->m_resolvedGPUResource->GetGPUVirtualAddress() + bufferPlatParams->m_heapByteOffset;
	}
}