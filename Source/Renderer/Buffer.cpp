// © 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "Buffer.h"
#include "Buffer_Platform.h"
#include "RenderManager.h"


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
			"We currently expect single-frame resources to be read-only (as we expect single-frame resources to always"
			"be in a read state, and can't transition to another state without transitioning the whole shared heap)");

		SEAssert(bufferParams.m_usageMask != re::Buffer::Usage::Invalid, "Invalid usage mask");
		
		SEAssert(((re::Buffer::HasUsageBit(re::Buffer::Constant, bufferParams) || 
			re::Buffer::HasUsageBit(re::Buffer::VertexStream, bufferParams) ||
			re::Buffer::HasUsageBit(re::Buffer::IndexStream, bufferParams)) &&
					bufferParams.m_arraySize == 1) ||
			(re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams) && 
				bufferParams.m_arraySize >= 1),
			"Invalid number of elements");

		SEAssert(!re::Buffer::HasUsageBit(re::Buffer::Constant, bufferParams) ||
			bufferParams.m_arraySize == 1,
			"Constant buffers only support a single element. Arrays are achieved as a member variable within a "
			"single constant buffer");
		
		SEAssert(bufferParams.m_stagingPool != re::Buffer::StagingPool::Permanent ||
			re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams),
			"GPU reads must be enabled for immutable buffers");

		SEAssert((re::Buffer::HasUsageBit(re::Buffer::Usage::VertexStream, bufferParams) && 
			re::Buffer::HasUsageBit(re::Buffer::Usage::IndexStream, bufferParams)) == false,
			"Buffer has both the vetex and index stream usage flags set. This is unexpected");
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
		, m_platformParams(nullptr)
		, m_isCurrentlyMapped(false)
	{
		SEAssert(m_dataByteSize % bufferParams.m_arraySize == 0,
			"Size must be non-zero, and equally divisible by the number of elements");
		
		ValidateBufferParams(m_bufferParams); // _DEBUG only

		platform::Buffer::CreatePlatformParams(*this);
	}


	void Buffer::Register(
		std::shared_ptr<re::Buffer> newBuffer, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == newBuffer->m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");

		re::Context::Get()->GetBufferAllocator()->RegisterAndAllocateBuffer(newBuffer, numBytes);

		RenderManager::Get()->RegisterForCreateDEPRECATED(newBuffer); // Enroll for deferred platform layer creation
	}


	void Buffer::RegisterAndCommit(
		std::shared_ptr<re::Buffer> newBuffer, void const* data, uint32_t numBytes, uint64_t typeIDHash)
	{
		Register(newBuffer, numBytes, typeIDHash);

		re::Context::Get()->GetBufferAllocator()->Commit(newBuffer->GetUniqueID(), data);

		newBuffer->m_platformParams->m_isCommitted = true;
	}


	void Buffer::CommitInternal(void const* data, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_bufferParams.m_stagingPool == StagingPool::Permanent, "Cannot set data of an immutable buffer");

		re::Context::Get()->GetBufferAllocator()->Commit(GetUniqueID(), data);
		
		m_platformParams->m_isCommitted = true;
	}


	void Buffer::CommitInternal(void const* data, uint32_t dstBaseOffset, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_bufferParams.m_stagingPool == re::Buffer::StagingPool::Permanent,
			"Only Permanent buffers can be partially updated");
		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, m_bufferParams),
			"Only structured buffers can be partially updated");

		re::Context::Get()->GetBufferAllocator()->Commit(GetUniqueID(), data, numBytes, dstBaseOffset);

		m_platformParams->m_isCommitted = true;
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
		re::Buffer::PlatformParams* params = GetPlatformParams();
		SEAssert(!params->m_isCreated,
			"Buffer destructor called, but buffer is still marked as created. Did a parameter "
			"block go out of scope without Destroy() being called?");
		SEAssert(!m_isCurrentlyMapped, "Buffer is currently mapped");
	}


	void Buffer::Destroy()
	{
		re::Buffer::PlatformParams* params = GetPlatformParams();
		SEAssert(params->m_isCreated, "Buffer has not been created, or has already been destroyed");
		SEAssert(!m_isCurrentlyMapped, "Buffer is currently mapped");

		// Internally makes a (deferred) call to platform::Buffer::Destroy
		re::Context::Get()->GetBufferAllocator()->Deallocate(GetUniqueID());
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