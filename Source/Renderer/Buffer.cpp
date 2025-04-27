// © 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "BindlessResource.h"
#include "Buffer.h"
#include "Buffer_Platform.h"
#include "RenderManager.h"

#include "Core/ProfilingMarkers.h"


namespace
{
	void ValidateBufferParams(re::Buffer::BufferParams const& bufferParams)
	{
#if defined(_DEBUG)
		SEAssert(bufferParams.m_stagingPool != re::Buffer::StagingPool::StagingPool_Invalid, "Invalid AllocationType");

		SEAssert(bufferParams.m_memPoolPreference != re::Buffer::UploadHeap ||
			(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams) &&
				re::Buffer::HasAccessBit(re::Buffer::CPUWrite, bufferParams)),
			"Buffers in the upload heap must be GPU-readable and CPU-writeable");

		SEAssert(!re::Buffer::HasAccessBit(re::Buffer::CPUWrite, bufferParams) ||
			bufferParams.m_memPoolPreference != re::Buffer::DefaultHeap,
			"Buffers in the default heap cannot have CPUWrite enabled");

		SEAssert(bufferParams.m_lifetime != re::Lifetime::SingleFrame ||
			bufferParams.m_memPoolPreference == re::Buffer::UploadHeap,
			"We currently expect single frame resources to be on the upload heap. This is NOT mandatory, we just need "
			"to implement support at the API level (i.e. BufferAllocator_DX12.h/.cpp)");

		SEAssert(bufferParams.m_lifetime != re::Lifetime::SingleFrame ||
			(bufferParams.m_stagingPool == re::Buffer::StagingPool::Temporary ||
				bufferParams.m_stagingPool == re::Buffer::StagingPool::None),
			"Single frame buffers can only use the temporary staging pool");

		SEAssert(!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams) ||
			(bufferParams.m_memPoolPreference == re::Buffer::DefaultHeap &&
				(bufferParams.m_stagingPool == re::Buffer::StagingPool::Temporary ||
					bufferParams.m_stagingPool == re::Buffer::StagingPool::None)),
			"If GPUWrite is enabled, buffers must be CPU-immutable and located in the default heap");

		SEAssert(!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams) || 
		bufferParams.m_lifetime != re::Lifetime::SingleFrame,
			"We currently expect single-frame resources to be read-only as any resource transitions will affect the "
			"entire backing resource");

		SEAssert(bufferParams.m_usageMask != re::Buffer::Usage::Invalid, "Invalid usage mask");
		
		SEAssert((re::Buffer::HasUsageBit(re::Buffer::Constant, bufferParams) &&
			bufferParams.m_arraySize == 1) ||
			((re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams) ||
				re::Buffer::HasUsageBit(re::Buffer::Raw, bufferParams)) &&
				bufferParams.m_arraySize >= 1),
			"Invalid number of elements. Arrays are only valid for Usage types with operator[] (not descriptor arrays)");

		SEAssert(bufferParams.m_stagingPool != re::Buffer::StagingPool::Permanent ||
			re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams),
			"GPU reads must be enabled for immutable buffers");
#endif
	}
}

namespace re
{
	// Private CTOR: Use one of the Create factories instead
	Buffer::Buffer(
		size_t typeIDHashCode, std::string const& bufferName, BufferParams const& bufferParams, uint32_t dataByteSize)
		: INamedObject(bufferName)
		, m_typeIDHash(typeIDHashCode)
		, m_dataByteSize(dataByteSize)
		, m_bufferParams(bufferParams)
		, m_platObj(nullptr)
		, m_cbvResourceHandle(k_invalidResourceHandle)
		, m_srvResourceHandle(k_invalidResourceHandle)
		, m_isCurrentlyMapped(false)
	{
		SEAssert(m_dataByteSize % bufferParams.m_arraySize == 0,
			"Size must be non-zero, and equally divisible by the number of elements");
		
		ValidateBufferParams(m_bufferParams); // _DEBUG only

		platform::Buffer::CreatePlatformObject(*this);

#if defined(_DEBUG)
		m_creationFrameNum = re::RenderManager::Get()->GetCurrentRenderFrameNum();
#endif
	}


	void Buffer::Register(
		std::shared_ptr<re::Buffer> const& newBuffer, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEBeginCPUEvent("Buffer::Register");

		SEAssert(typeIDHash == newBuffer->m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");

		// Get a bindless resource handle:
		re::BindlessResourceManager* brm = re::Context::Get()->GetBindlessResourceManager();
		if (brm) // May be null (e.g. API does not support bindless resources)
		{
			if (HasUsageBit(re::Buffer::Usage::Constant, *newBuffer))
			{
				newBuffer->m_cbvResourceHandle = brm->RegisterResource(
					std::make_unique<re::BufferResource>(newBuffer, re::ViewType::CBV));
			}

			// Note: Buffers with Raw usage (e.g. VertexStreams) can be larger than what is allowed for a CBV, so we
			// only create a SRV handle for them
			if (HasUsageBit(re::Buffer::Usage::Structured, *newBuffer) ||
				HasUsageBit(re::Buffer::Usage::Raw, *newBuffer))
			{
				newBuffer->m_srvResourceHandle = brm->RegisterResource(
					std::make_unique<re::BufferResource>(newBuffer, re::ViewType::SRV));
			}
		}

		re::Context::Get()->GetBufferAllocator()->Register(newBuffer, numBytes);

		SEEndCPUEvent();
	}


	void Buffer::RegisterAndCommit(
		std::shared_ptr<re::Buffer> const& newBuffer, void const* data, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEBeginCPUEvent("Buffer::RegisterAndCommit");

		Register(newBuffer, numBytes, typeIDHash);

		re::Context::Get()->GetBufferAllocator()->Stage(newBuffer->GetUniqueID(), data);

		newBuffer->m_platObj->m_isCommitted = true;

		SEEndCPUEvent();
	}


	void Buffer::CommitInternal(void const* data, uint64_t typeIDHash)
	{
		SEBeginCPUEvent("Buffer::CommitInternal");

		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_bufferParams.m_stagingPool == StagingPool::Permanent, "Cannot set data of an immutable buffer");

		re::Context::Get()->GetBufferAllocator()->Stage(GetUniqueID(), data);
		
		m_platObj->m_isCommitted = true;

		SEEndCPUEvent();
	}


	void Buffer::CommitMutableInternal(void const* data, uint32_t dstBaseOffset, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_bufferParams.m_stagingPool == re::Buffer::StagingPool::Permanent,
			"Only Permanent buffers can be partially updated");
		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, m_bufferParams) ||
			re::Buffer::HasUsageBit(re::Buffer::Raw, m_bufferParams),
			"Invalid buffer usage for partial updates");

		re::Context::Get()->GetBufferAllocator()->StageMutable(GetUniqueID(), data, numBytes, dstBaseOffset);

		m_platObj->m_isCommitted = true;
	}


	void const* Buffer::GetData() const
	{
		void const* dataOut = nullptr;
		re::Context::Get()->GetBufferAllocator()->GetData(GetUniqueID(), &dataOut);
		return dataOut;
	}


	void Buffer::GetDataAndSize(void const** out_data, uint32_t* out_numBytes) const
	{
		re::Context::Get()->GetBufferAllocator()->GetData(GetUniqueID(), out_data);

		*out_numBytes = m_dataByteSize;
	}


	Buffer::~Buffer()
	{
		SEAssert(!m_isCurrentlyMapped, "Buffer is currently mapped");

#if defined(_DEBUG)
		SEAssert(m_bufferParams.m_lifetime != re::Lifetime::SingleFrame ||
			m_creationFrameNum == re::RenderManager::Get()->GetCurrentRenderFrameNum(),
			"Single frame buffer created on frame %llu being destroyed on frame %llu. Does something still hold the "
			"buffer beyond its lifetime? E.g. Has a single-frame batch been added to a stage, but the stage is not "
			"added to the pipeline (thus has not been cleared)?",
			m_creationFrameNum,
			re::RenderManager::Get()->GetCurrentRenderFrameNum());
#endif

		// Free bindless resource handles:
		if (m_srvResourceHandle != k_invalidResourceHandle)
		{
			re::BindlessResourceManager* brm = re::Context::Get()->GetBindlessResourceManager();
			SEAssert(brm,
				"Failed to get BindlessResourceManager, but resource handle is valid. This should not be possible");

			brm->UnregisterResource(m_srvResourceHandle, re::RenderManager::Get()->GetCurrentRenderFrameNum());
		}

		if (m_cbvResourceHandle != k_invalidResourceHandle)
		{
			re::BindlessResourceManager* brm = re::Context::Get()->GetBindlessResourceManager();
			SEAssert(brm,
				"Failed to get BindlessResourceManager, but resource handle is valid. This should not be possible");

			brm->UnregisterResource(m_cbvResourceHandle, re::RenderManager::Get()->GetCurrentRenderFrameNum());
		}

		if (m_platObj->m_isCreated)
		{
			re::Context::Get()->GetBufferAllocator()->Deallocate(GetUniqueID());

			re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platObj));
		}		
	}


	void const* Buffer::MapCPUReadback(uint8_t frameLatency /*= k_maxFrameLatency*/)
	{
		SEAssert(re::Buffer::HasAccessBit(re::Buffer::CPURead, m_bufferParams), "CPU reads are not enabled");
		SEAssert(!m_isCurrentlyMapped, "Buffer is already mapped. Did you forget to unmap it during an earlier frame?");

		re::RenderManager const* renderManager = re::RenderManager::Get();

		// Convert the default frame latency value:
		if (frameLatency == re::Buffer::k_maxFrameLatency)
		{
			const uint8_t numFramesInFlight = renderManager->GetNumFramesInFlight();
			frameLatency = numFramesInFlight - 1;
		}
		SEAssert(frameLatency > 0 && frameLatency < renderManager->GetNumFramesInFlight(),
			"Invalid frame latency");

		// Ensure we've got results to retrieve:
		const uint64_t currentRenderFrameNum = renderManager->GetCurrentRenderFrameNum();
		if (currentRenderFrameNum < frameLatency)
		{
			return nullptr; // There is nothing to read back for the first (numFramesInFlight - 1) frames
		}

		// Get the mapped data:
		void const* mappedData = platform::Buffer::MapCPUReadback(*this, frameLatency);
		if (mappedData)
		{
			m_isCurrentlyMapped = true;
		}
		return mappedData;
	}


	void Buffer::UnmapCPUReadback()
	{
		SEAssert(re::Buffer::HasAccessBit(re::Buffer::CPURead, m_bufferParams), "CPU reads are not enabled");
		SEAssert(m_isCurrentlyMapped, "Buffer is not currently mapped");

		platform::Buffer::UnmapCPUReadback(*this);

		m_isCurrentlyMapped = false;
	}
}