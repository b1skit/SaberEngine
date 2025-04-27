// © 2022 Adam Badke. All rights reserved.
#include "Buffer_DX12.h"
#include "BufferAllocator_DX12.h"
#include "BufferView.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "EnumTypes_DX12.h"
#include "Fence_DX12.h"
#include "RenderManager.h"

#include "Core/Assert.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/MathUtils.h"

#include <d3dx12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;


namespace
{
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


	// Translate a BufferView to map to the backing GPU resource
	re::BufferView TranslateBufferView(re::Buffer const* buffer, re::BufferView const& view, uint64_t baseByteOffset)
	{
		const uint64_t alignedSize =
			dx12::Buffer::GetAlignedSize(buffer->GetBufferParams().m_usageMask, buffer->GetTotalBytes());

		// We translate our views for buffers in shared heaps or buffers with N-buffered allocations by assuming the
		// entire resource is filled with the same type of data, and computing our first element offset to match
		if (view.IsVertexStreamView())
		{
			const uint32_t firstElement =
				util::CheckedCast<uint32_t>(baseByteOffset / alignedSize) + view.m_streamView.m_firstElement;

			return re::BufferView(re::BufferView::VertexStreamType{
				.m_firstElement = firstElement,
				.m_numElements = view.m_streamView.m_numElements,
				.m_type = view.m_streamView.m_type,
				.m_dataType = view.m_streamView.m_dataType,
				.m_isNormalized = view.m_streamView.m_isNormalized,
			});
		}
		else
		{
			const uint32_t firstElement =
				util::CheckedCast<uint32_t>(baseByteOffset / alignedSize) + view.m_bufferView.m_firstElement;

			return re::BufferView(re::BufferView::BufferType{
				.m_firstElement = firstElement,
				.m_numElements = view.m_bufferView.m_numElements,
				.m_structuredByteStride = view.m_bufferView.m_structuredByteStride,
				.m_firstDestIdx = view.m_bufferView.m_firstDestIdx,
			});
		}
	}


	// Clamp the frame offset index for Buffers that only have a single backing resource
	void ClampFrameOffsetIdx(re::Buffer const* buffer, uint8_t& frameOffsetIdx)
	{
		SEStaticAssert(static_cast<uint8_t>(re::Buffer::StagingPool::StagingPool_Invalid) == 3u,
			"Number of staging pools has changed. This must be updated");

		SEAssert(frameOffsetIdx >= 0 && frameOffsetIdx < 3, "Unexpected frame offset index");

		if (buffer->GetLifetime() == re::Lifetime::SingleFrame || 
			buffer->GetStagingPool() != re::Buffer::StagingPool::Permanent) // i.e. Temporary, or none
		{
			frameOffsetIdx = 0;
		}
	}
}


namespace dx12
{
	Buffer::PlatObj::PlatObj()
		: m_gpuResource(nullptr)
		, m_resolvedGPUResource(nullptr)
		, m_baseByteOffset(0)
		, m_currentMapFrameLatency(std::numeric_limits<uint8_t>::max())
		, m_srvDescriptors(dx12::DescriptorCache::DescriptorType::SRV)
		, m_uavDescriptors(dx12::DescriptorCache::DescriptorType::UAV)
		, m_cbvDescriptors(dx12::DescriptorCache::DescriptorType::CBV)
		, m_views{ 0 }
	{
	}


	Buffer::PlatObj::~PlatObj()
	{
		SEAssert(!m_isCreated, "Buffer destructor called before Destroy()");

		m_srvDescriptors.Destroy();
		m_uavDescriptors.Destroy();
		m_cbvDescriptors.Destroy();
	}


	void Buffer::PlatObj::Destroy()
	{
		SEAssert(m_isCreated, "Attempting to destroy a Buffer that has not been created");

		m_isCreated = false;

		SEAssert((m_gpuResource && m_gpuResource->IsValid()) ||
			(!m_gpuResource && m_resolvedGPUResource != nullptr),
			"GPUResource should be valid");

		m_gpuResource = nullptr;
		m_resolvedGPUResource = nullptr;
		m_baseByteOffset = 0;
	}


	void Buffer::Create(re::Buffer& buffer)
	{
		re::Buffer::BufferParams const& bufferParams = buffer.GetBufferParams();
		
		SEAssert(!re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams) ||
			bufferParams.m_arraySize <= 1024, "Maximum offset of 1024 allowed into an SRV");

		dx12::Buffer::PlatObj* platObj = buffer.GetPlatformObject()->As<dx12::Buffer::PlatObj*>();
		SEAssert(!platObj->m_isCreated, "Buffer is already created");
		platObj->m_isCreated = true;

		const uint8_t numFramesInFlight = re::RenderManager::GetNumFramesInFlight();

		const re::Lifetime bufferLifetime = buffer.GetLifetime();

		const bool needsUAV = NeedsUAV(bufferParams);

		uint32_t totalBytes = buffer.GetTotalBytes();

		// Single frame buffers sub-allocated from a single resource:
		if (bufferLifetime == re::Lifetime::SingleFrame &&
			bufferParams.m_memPoolPreference == re::Buffer::MemoryPoolPreference::UploadHeap &&
			!needsUAV)
		{
			dx12::BufferAllocator* bufferAllocator =
				dynamic_cast<dx12::BufferAllocator*>(re::Context::Get()->GetBufferAllocator());

			bufferAllocator->GetSubAllocation(
				bufferParams.m_usageMask,
				GetAlignedSize(bufferParams.m_usageMask, totalBytes),
				platObj->m_baseByteOffset,
				platObj->m_resolvedGPUResource);

			SEAssert(platObj->m_baseByteOffset % GetAlignment(
				re::BufferAllocator::BufferUsageMaskToAllocationPool(bufferParams.m_usageMask)) == 0,
				"Base offset does not have the correct buffer alignment");
		}
		else // Placed resources via the heap manager:
		{
			if (buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent)
			{
				// We allocate N aligned frames-worth of buffer space, and then set the m_baseByteOffset each frame
				totalBytes = util::CheckedCast<uint32_t>(
					GetAlignedSize(bufferParams.m_usageMask, totalBytes) * numFramesInFlight);
			}

			CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
				GetAlignedSize(bufferParams.m_usageMask, totalBytes));
			
			if (needsUAV)
			{
				bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			std::wstring const& debugName = CreateDebugName(buffer);

			platObj->m_gpuResource = re::Context::GetAs<dx12::Context*>()->GetHeapManager().CreateResource(
				dx12::ResourceDesc{
					.m_resourceDesc = bufferDesc,
					.m_heapType = MemoryPoolPreferenceToD3DHeapType(bufferParams.m_memPoolPreference),
					.m_initialState = D3D12_RESOURCE_STATE_COMMON,
				},
				debugName.c_str());

			platObj->m_resolvedGPUResource = platObj->m_gpuResource->Get();
		}
		
		// CPU readback:
		const bool cpuReadbackEnabled = re::Buffer::HasAccessBit(re::Buffer::CPURead, bufferParams);
		if (cpuReadbackEnabled)
		{
			for (uint8_t resourceIdx = 0; resourceIdx < numFramesInFlight; resourceIdx++)
			{
				std::wstring const& readbackDebugName = 
					buffer.GetWName() + L"_ReadbackBuffer" + std::to_wstring(resourceIdx);

				platObj->m_readbackResources.emplace_back(
					CreateReadbackResource(buffer.GetTotalBytes(), readbackDebugName.c_str()));
			}
		}
	}


	void Buffer::Update(
		re::Buffer const& buffer, uint8_t curFrameHeapOffsetFactor, uint32_t commitBaseOffset, uint32_t numBytes)
	{
		re::Buffer::BufferParams const& bufferParams = buffer.GetBufferParams();

		SEAssert(re::Buffer::HasAccessBit(re::Buffer::CPUWrite, bufferParams) &&
			bufferParams.m_memPoolPreference == re::Buffer::UploadHeap,
			"CPU writes must be enabled to allow mapping");

		dx12::Buffer::PlatObj* platObj = buffer.GetPlatformObject()->As<dx12::Buffer::PlatObj*>();

		// We map and then unmap immediately; Microsoft recommends resources be left unmapped while the CPU will not 
		// modify them, and use tight, accurate ranges at all times
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map
		void const* srcData = nullptr;
		uint32_t totalBytes = 0;
		buffer.GetDataAndSize(&srcData, &totalBytes);

		// Update the heap offset, if required
		if (buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent)
		{
			const uint64_t alignedSize = GetAlignedSize(bufferParams.m_usageMask, totalBytes);
			platObj->m_baseByteOffset = alignedSize * curFrameHeapOffsetFactor;
		}

		// Get a CPU pointer to the subresource (i.e subresource 0)
		void* cpuVisibleData = nullptr;
		const D3D12_RANGE readRange{ .Begin = 0, .End = 0 }; // We won't read from this resource on the CPU (end <= begin)
		CheckHResult(platObj->m_resolvedGPUResource->Map(
				0,									// Subresource index: Buffers only have a single subresource
				&readRange,
				&cpuVisibleData),
			"Buffer::Update: Failed to map committed resource");

		const bool updateAllBytes = commitBaseOffset == 0 && (numBytes == 0 || numBytes == totalBytes);
		SEAssert(updateAllBytes || (commitBaseOffset + numBytes <= totalBytes),
			"Base offset and number of bytes are out of bounds");

		// Adjust our pointers if we're doing a partial update:
		if (!updateAllBytes)
		{
			SEAssert(buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent,
				"Only mutable buffers can be partially updated");

			// Update the source data pointer:
			srcData = static_cast<uint8_t const*>(srcData) + commitBaseOffset;
			totalBytes = numBytes;

			// Update the destination pointer:
			cpuVisibleData = static_cast<uint8_t*>(cpuVisibleData) + commitBaseOffset;
		}
		SEAssert(!updateAllBytes || commitBaseOffset == 0, "Invalid base offset");

		// Copy our data to the appropriate offset in the cpu-visible heap:
		void* offsetPtr = static_cast<uint8_t*>(cpuVisibleData) + platObj->m_baseByteOffset;
		memcpy(offsetPtr, srcData, totalBytes);
	
		// Release the map:
		const D3D12_RANGE writtenRange{
			platObj->m_baseByteOffset + commitBaseOffset,
			platObj->m_baseByteOffset + commitBaseOffset + totalBytes };

		platObj->m_resolvedGPUResource->Unmap(
			0,					// Subresource index: Buffers only have a single subresource
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource
	}


	void Buffer::Update(
		re::Buffer const* buffer,
		uint8_t frameOffsetIdx,
		uint32_t commitBaseOffset,
		uint32_t numBytes,
		dx12::CommandList* copyCmdList,
		dx12::GPUResource* intermediateResource,
		uint64_t alignedItermediateBaseOffset)
	{
		SEAssert(numBytes > 0, "Invalid update size");

		dx12::Buffer::PlatObj* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj*>();

		SEAssert(alignedItermediateBaseOffset % GetAlignment(
				re::BufferAllocator::BufferUsageMaskToAllocationPool(buffer->GetUsageMask())) == 0,
			"Invalid intermediate resource base offset");

		// Update the heap offset, if required
		if (buffer->GetStagingPool() == re::Buffer::StagingPool::Permanent)
		{
			dx12::Buffer::PlatObj* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj*>();

			const uint64_t alignedSize = GetAlignedSize(buffer->GetBufferParams().m_usageMask, buffer->GetTotalBytes());

			platObj->m_baseByteOffset = alignedSize * frameOffsetIdx;
		}

		constexpr uint32_t k_intermediateSubresourceIdx = 0;

		// Map the intermediate resource, and copy our data into it
		void* cpuVisibleData = nullptr;
		const CD3DX12_RANGE readRange(0, 0);	// We don't read from this resource on the CPU (end <= begin)
		const HRESULT hr = intermediateResource->Map(
			k_intermediateSubresourceIdx,
			&readRange,
			&cpuVisibleData);
		CheckHResult(hr, "Buffer::Update: Failed to map intermediate committed resource");

		// Offset our mapping into the intermediate resource:
		cpuVisibleData = static_cast<uint8_t*>(cpuVisibleData) + alignedItermediateBaseOffset;

		// Copy our data to the appropriate offset in the cpu-visible heap:
		void const* const srcData = static_cast<uint8_t const*>(buffer->GetData()) + commitBaseOffset;
		memcpy(cpuVisibleData, srcData, numBytes);

		// Release the map:
		const D3D12_RANGE writtenRange{
			.Begin = alignedItermediateBaseOffset,
			.End = numBytes };

		intermediateResource->Unmap(
			k_intermediateSubresourceIdx,
			&writtenRange);		// Unmap range: The region the CPU may have modified. Nullptr = entire subresource

		// Schedule a copy from the intermediate resource to default/L1/vid memory heap via the copy queue:
		const uint32_t dstOffset = util::CheckedCast<uint32_t>(platObj->m_baseByteOffset + commitBaseOffset);
		copyCmdList->UpdateSubresources(
			buffer, dstOffset, intermediateResource->Get(), alignedItermediateBaseOffset, numBytes);
	}


	void const* Buffer::MapCPUReadback(re::Buffer const& buffer, uint8_t frameLatency)
	{
		dx12::Buffer::PlatObj* platObj = buffer.GetPlatformObject()->As<dx12::Buffer::PlatObj*>();
		re::RenderManager const* renderManager = re::RenderManager::Get();

		const uint32_t bufferSize = buffer.GetTotalBytes();

		// Compute the index of the readback resource we're mapping:
		SEAssert(renderManager->GetCurrentRenderFrameNum() >= frameLatency, "Frame latency would result in OOB access");
		const uint8_t readbackResourceIdx =
			(renderManager->GetCurrentRenderFrameNum() - frameLatency) % renderManager->GetNumFramesInFlight();

		// Ensure the GPU is finished with the buffer:
		{
			std::lock_guard<std::mutex> lock(platObj->m_readbackResources[readbackResourceIdx].m_readbackFenceMutex);

			const dx12::CommandListType resourceCopyCmdListType = dx12::Fence::GetCommandListTypeFromFenceValue(
				platObj->m_readbackResources[readbackResourceIdx].m_readbackFence);

			dx12::CommandQueue& resourceCopyQueue =
				re::Context::GetAs<dx12::Context*>()->GetCommandQueue(resourceCopyCmdListType);

			resourceCopyQueue.CPUWait(platObj->m_readbackResources[readbackResourceIdx].m_readbackFence);
		}

		const D3D12_RANGE readbackBufferRange{
			0,				// Begin
			bufferSize };	// End
		
		void* cpuVisibleData = nullptr;

		HRESULT hr = platObj->m_readbackResources[readbackResourceIdx].m_readbackGPUResource->Map(
			0,						// Subresource
			&readbackBufferRange,	// pReadRange
			&cpuVisibleData);		// ppData
		CheckHResult(hr, "Buffer::MapCPUReadback: Failed to map readback resource");

		platObj->m_currentMapFrameLatency = frameLatency;

		return cpuVisibleData;
	}


	void Buffer::UnmapCPUReadback(re::Buffer const& buffer)
	{
		dx12::Buffer::PlatObj* platObj = buffer.GetPlatformObject()->As<dx12::Buffer::PlatObj*>();
		re::RenderManager const* renderManager = re::RenderManager::Get();

		// Compute the index of the readback resource we're unmapping:
		SEAssert(renderManager->GetCurrentRenderFrameNum() >= platObj->m_currentMapFrameLatency,
			"Frame latency would result in OOB access");
		const uint8_t readbackResourceIdx =
			(renderManager->GetCurrentRenderFrameNum() - platObj->m_currentMapFrameLatency) % renderManager->GetNumFramesInFlight();

		const D3D12_RANGE writtenRange{
			0,		// Begin
			0 };	// End: Signifies CPU did not write any data when End <= Begin

		platObj->m_readbackResources[readbackResourceIdx].m_readbackGPUResource->Unmap(
			0,				// Subresource
			&writtenRange);	// pWrittenRange
	}


	uint64_t Buffer::GetAlignedSize(re::Buffer::UsageMask usageMask, uint32_t bufferSize)
	{
		return util::RoundUpToNearestMultiple<uint64_t>(
			bufferSize,
			GetAlignment(re::BufferAllocator::BufferUsageMaskToAllocationPool(usageMask)));
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetCBV(
		re::Buffer const* buffer, re::BufferView const& view, uint8_t frameOffsetIdx)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();
		SEAssert(platObj->m_isCreated == true, "Platform object has not been created");

		uint64_t baseByteOffset = 0;
		if (IsInSharedHeap(buffer))
		{
			baseByteOffset = platObj->GetBaseByteOffset();
		}
		else
		{
			ClampFrameOffsetIdx(buffer, frameOffsetIdx);

			const uint64_t alignedSize =
				dx12::Buffer::GetAlignedSize(buffer->GetBufferParams().m_usageMask, buffer->GetTotalBytes());

			baseByteOffset = alignedSize * frameOffsetIdx;
		}

		return GetCBVInternal(buffer, view, baseByteOffset);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetSRV(
		re::Buffer const* buffer, re::BufferView const& view, uint8_t frameOffsetIdx)
	{
		SEAssert(buffer, "Buffer cannot be null");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();
		SEAssert(platObj->m_isCreated == true, "Platform object has not been created");

		uint64_t baseByteOffset = 0;
		if (IsInSharedHeap(buffer))
		{
			baseByteOffset = platObj->GetBaseByteOffset();
		}
		else
		{
			ClampFrameOffsetIdx(buffer, frameOffsetIdx);

			const uint64_t alignedSize =
				dx12::Buffer::GetAlignedSize(buffer->GetBufferParams().m_usageMask, buffer->GetTotalBytes());

			baseByteOffset = alignedSize * frameOffsetIdx;
		}

		return GetSRVInternal(buffer, view, baseByteOffset);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetUAV(
		re::Buffer const* buffer, re::BufferView const& view, uint8_t frameOffsetIdx)
	{
		SEAssert(buffer, "Buffer cannot be null");
		SEAssert(IsInSharedHeap(buffer) == false, "Buffer is in a shared heap. This is unexpected for a UAV");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();
		SEAssert(platObj->m_isCreated == true, "Platform object has not been created");
		
		ClampFrameOffsetIdx(buffer, frameOffsetIdx);

		const uint64_t alignedSize =
			dx12::Buffer::GetAlignedSize(buffer->GetBufferParams().m_usageMask, buffer->GetTotalBytes());

		const uint64_t baseByteOffset = alignedSize * frameOffsetIdx;

		return GetUAVInternal(buffer, view, baseByteOffset);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetCBVInternal(
		re::Buffer const* buffer, re::BufferView const& view, uint64_t baseByteOffset)
	{
		SEAssert(buffer, "Buffer cannot be null");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, buffer->GetBufferParams()),
			"Buffer is missing the Constant usage bit");

		SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, buffer->GetBufferParams()) &&
			!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, buffer->GetBufferParams()),
			"Invalid usage flags for a constant buffer");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();

		SEAssert(platObj->m_isCreated == true, "Platform object has not been created");

		return platObj->m_cbvDescriptors.GetCreateDescriptor(
			buffer,
			TranslateBufferView(buffer, view, baseByteOffset));
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetSRVInternal(
		re::Buffer const* buffer, re::BufferView const& view, uint64_t baseByteOffset)
	{
		SEAssert(buffer, "Buffer cannot be null");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, buffer->GetBufferParams()) ||
			re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer->GetBufferParams()),
			"Buffer is missing the Structured usage bit");
		SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, buffer->GetBufferParams()),
			"SRV buffers must have GPU reads enabled");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();

		SEAssert(platObj->m_isCreated == true, "Platform object has not been created");

		return platObj->m_srvDescriptors.GetCreateDescriptor(
			buffer,
			TranslateBufferView(buffer, view, baseByteOffset));
	}


	D3D12_CPU_DESCRIPTOR_HANDLE Buffer::GetUAVInternal(
		re::Buffer const* buffer, re::BufferView const& view, uint64_t baseByteOffset)
	{
		SEAssert(buffer, "Buffer cannot be null");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, buffer->GetBufferParams()) ||
			re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer->GetBufferParams()),
			"Buffer is missing the Structured usage bit");
		SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPUWrite, buffer->GetBufferParams()),
			"UAV buffers must have GPU writes enabled");

		dx12::Buffer::PlatObj const* platObj = buffer->GetPlatformObject()->As<dx12::Buffer::PlatObj const*>();

		SEAssert(platObj->m_isCreated == true, "Platform object has not been created");

		return platObj->m_uavDescriptors.GetCreateDescriptor(
			buffer,
			TranslateBufferView(buffer, view, baseByteOffset));
	}


	D3D12_VERTEX_BUFFER_VIEW const* dx12::Buffer::GetOrCreateVertexBufferView(
		re::Buffer const& buffer, re::BufferView const& view)
	{
		SEAssert(view.IsVertexStreamView(), "Invalid view type");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer),
			"Buffer does not have the correct usage flags set");

		SEAssert(view.m_streamView.m_dataType != re::DataType::DataType_Count &&
			view.m_streamView.m_dataType >= re::DataType::Float && 
			view.m_streamView.m_dataType <= re::DataType::UByte4,
			"Invalid data type");

		dx12::Buffer::PlatObj* platObj = buffer.GetPlatformObject()->As<dx12::Buffer::PlatObj*>();

		if (platObj->m_views.m_vertexBufferView.BufferLocation == 0) // Has not been created yet
		{
			std::unique_lock<std::mutex> lock(platObj->m_viewMutex);

			if (platObj->m_views.m_vertexBufferView.BufferLocation == 0) // Confirm: Still not created
			{
				const uint32_t byteStride = DataTypeToByteStride(view.m_streamView.m_dataType);
				const uint32_t sizeInBytes = buffer.GetTotalBytes() - (byteStride * view.m_streamView.m_firstElement);

				platObj->m_views.m_vertexBufferView = D3D12_VERTEX_BUFFER_VIEW{
					.BufferLocation = platObj->GetGPUVirtualAddress(view),
					.SizeInBytes = sizeInBytes,
					.StrideInBytes = byteStride,
				};
			}
		}

		return &platObj->m_views.m_vertexBufferView;
	}


	D3D12_INDEX_BUFFER_VIEW const* dx12::Buffer::GetOrCreateIndexBufferView(
		re::Buffer const& buffer, re::BufferView const& view)
	{
		SEAssert(view.IsVertexStreamView(), "Invalid view type");

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Raw, buffer),
			"Buffer does not have the correct usage flags set");

		dx12::Buffer::PlatObj* platObj = buffer.GetPlatformObject()->As<dx12::Buffer::PlatObj*>();

		if (platObj->m_views.m_indexBufferView.BufferLocation == 0) // Has not been created yet
		{
			std::unique_lock<std::mutex> lock(platObj->m_viewMutex);

			if (platObj->m_views.m_indexBufferView.BufferLocation == 0) // Confirm: Still not created
			{
				const uint32_t byteStride = DataTypeToByteStride(view.m_streamView.m_dataType);
				const uint32_t sizeInBytes = buffer.GetTotalBytes() - (byteStride * view.m_streamView.m_firstElement);

				platObj->m_views.m_indexBufferView = D3D12_INDEX_BUFFER_VIEW{
					.BufferLocation = platObj->GetGPUVirtualAddress(view),
					.SizeInBytes = sizeInBytes,
					.Format = dx12::DataTypeToDXGI_FORMAT(view.m_streamView.m_dataType, false),
				};
			}
		}

		return &platObj->m_views.m_indexBufferView;
	}
}