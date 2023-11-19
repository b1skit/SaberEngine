// � 2022 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "DebugConfiguration.h"
#include "ParameterBlockAllocator.h"
#include "ParameterBlockAllocator_Platform.h"
#include "ParameterBlock_Platform.h"
#include "ProfilingMarkers.h"
#include "RenderManager.h"
#include "RenderManager_Platform.h"

using re::ParameterBlock;
using std::shared_ptr;
using std::unordered_map;


namespace
{
	// How many frames-worth of padding before calling ParameterBlock::Destroy on deallocated PBs
	constexpr uint64_t k_deferredDeleteNumFrames = 2;
}

namespace re
{
	// Parameter Block Platform Params:
	//---------------------------------

	ParameterBlockAllocator::PlatformParams::PlatformParams()
		: m_numBuffers(platform::RenderManager::GetNumFrames())
		, m_writeIdx(0)
	{
		// We maintain N stack base indexes for each PBDataType; Initialize them to 0
		for (uint8_t pbDataType = 0; pbDataType < re::ParameterBlock::PBDataType::PBDataType_Count; pbDataType++)
		{
			m_bufferBaseIndexes[pbDataType].store(0);
		}
	}


	void ParameterBlockAllocator::PlatformParams::BeginFrame()
	{
		// Increment the write index
		m_writeIdx = (m_writeIdx + 1) % m_numBuffers;

		// Reset the stack base index back to 0 for each type of shared PB buffer:
		for (uint8_t pbDataType = 0; pbDataType < re::ParameterBlock::PBDataType::PBDataType_Count; pbDataType++)
		{
			m_bufferBaseIndexes[pbDataType].store(0);
		}
	}


	uint32_t ParameterBlockAllocator::PlatformParams::AdvanceBaseIdx(
		re::ParameterBlock::PBDataType pbDataType, uint32_t alignedSize)
	{
		// Atomically advance the stack base index for the next call, and return the base index for the current one
		const uint32_t allocationBaseIdx = m_bufferBaseIndexes[pbDataType].fetch_add(alignedSize);

		SEAssert("Allocation is out of bounds. Consider increasing k_singleFrameBufferSize", 
			allocationBaseIdx + alignedSize <= k_fixedAllocationByteSize);

		return allocationBaseIdx;
	}


	uint8_t ParameterBlockAllocator::PlatformParams::GetWriteIndex() const
	{
		return m_writeIdx;
	}


	// Parameter Block Allocator:
	//---------------------------

	ParameterBlockAllocator::PlatformParams* ParameterBlockAllocator::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	void ParameterBlockAllocator::SetPlatformParams(std::unique_ptr<ParameterBlockAllocator::PlatformParams> params)
	{
		m_platformParams = std::move(params);
	}


	ParameterBlockAllocator::ParameterBlockAllocator()
		: m_immutableAllocations{}
		, m_mutableAllocations{}
		, m_singleFrameAllocations{}
		, m_maxSingleFrameAllocations(0) // Debug: Track the high-water mark for the max single-frame PB allocations
		, m_maxSingleFrameAllocationByteSize(0)
		, m_allocationPeriodEnded(false)
		, m_permanentPBsHaveBeenBuffered(false)
		, m_isValid(true)
		, m_readFrameNum(std::numeric_limits<uint64_t>::max()) // Odd 1st number (18,446,744,073,709,551,615), then wrap
	{
		platform::ParameterBlockAllocator::CreatePlatformParams(*this);

		// Initialize the single frame stack allocations:
		for (uint8_t i = 0; i < k_numBuffers; i++)
		{
			m_singleFrameAllocations.m_baseIdx[i] = 0;
			memset(&m_singleFrameAllocations.m_committed[i][0], 0, k_fixedAllocationByteSize); // Not actually necessary
		}
	}


	void ParameterBlockAllocator::Create()
	{
		platform::ParameterBlockAllocator::Create(*this);
	}


	ParameterBlockAllocator::~ParameterBlockAllocator()
	{
		SEAssert("Parameter block allocator destructor called before Destroy(). The parameter block allocator must "
			"be manually destroyed (i.e. in the api-specific Context::Destroy())", !IsValid());
	}
	

	void ParameterBlockAllocator::Destroy()
	{
		std::scoped_lock lock(
			m_uniqueIDToTypeAndByteIndexMutex,
			m_immutableAllocations.m_mutex, 
			m_mutableAllocations.m_mutex,
			m_singleFrameAllocations.m_mutex,
			m_dirtyParameterBlocksMutex);

		LOG(std::format("ParameterBlockAllocator shutting down. Session usage statistics:\n"
			"\t{} Immutable permanent allocations: {} B\n"
			"\t{} Mutable permanent allocations: {} B\n"
			"\t{} max single-frame allocations, max {} B buffer usage seen from fixed {} B buffer",
			m_immutableAllocations.m_handleToPtr.size(), 
			m_immutableAllocations.m_committed.size(),
			m_mutableAllocations.m_handleToPtr.size(),
			m_mutableAllocations.m_committed[0].size(),
			m_maxSingleFrameAllocations,
			m_maxSingleFrameAllocationByteSize,
			k_fixedAllocationByteSize
		).c_str());

		// Must clear the parameter blocks shared_ptrs before clearing the committed memory

		auto immutableItr = m_immutableAllocations.m_handleToPtr.begin();
		while (immutableItr != m_immutableAllocations.m_handleToPtr.end())
		{
			// Destroy() removes the ParameterBlock from our unordered_map; we must advance our iterators first
			re::ParameterBlock* curPB = immutableItr->second.get();
			immutableItr++;
			curPB->Destroy(); 
		}
		m_immutableAllocations.m_handleToPtr.clear();
		m_immutableAllocations.m_committed.clear();

		auto mutableItr = m_mutableAllocations.m_handleToPtr.begin();
		while (mutableItr != m_mutableAllocations.m_handleToPtr.end())
		{
			re::ParameterBlock* curPB = mutableItr->second.get();
			mutableItr++;
			curPB->Destroy();
		}
		m_mutableAllocations.m_handleToPtr.clear();

		for (uint8_t i = 0; i < k_numBuffers; i++)
		{
			m_mutableAllocations.m_committed[i].clear();

			auto singleFrameItr = m_singleFrameAllocations.m_handleToPtr[i].begin();
			while (singleFrameItr != m_singleFrameAllocations.m_handleToPtr[i].end())
			{
				re::ParameterBlock* curPB = singleFrameItr->second.get();
				singleFrameItr++;
				curPB->Destroy();
			}
			m_singleFrameAllocations.m_baseIdx[i] = 0;
			m_singleFrameAllocations.m_handleToPtr[i].clear();
		}

		// Clear the handle -> commit map
		m_uniqueIDToTypeAndByteIndex.clear();

		// The platform::RenderManager has already flushed all outstanding work; Force our deferred deletions to be
		// immediately cleared
		ClearDeferredDeletions(std::numeric_limits<uint64_t>::max());

		platform::ParameterBlockAllocator::Destroy(*this);

		m_isValid = false;
	}


	bool ParameterBlockAllocator::IsValid() const
	{
		return m_isValid;
	}


	void ParameterBlockAllocator::ClosePermanentPBRegistrationPeriod()
	{
		m_allocationPeriodEnded = true;
	}


	void ParameterBlockAllocator::RegisterAndAllocateParameterBlock(
		std::shared_ptr<re::ParameterBlock> pb, uint32_t numBytes)
	{
		SEAssert("Permanent parameter blocks can only be registered at startup, before the 1st render frame", 
			pb->GetType() == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded);

		const ParameterBlock::PBType pbType = pb->GetType();
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			const size_t writeIdx = GetWriteIdx();

			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			SEAssert("Parameter block is already registered",
				!m_singleFrameAllocations.m_handleToPtr[writeIdx].contains(pb->GetUniqueID()));
			m_singleFrameAllocations.m_handleToPtr[writeIdx][pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);

			SEAssert("Parameter block is already registered", 
				!m_immutableAllocations.m_handleToPtr.contains(pb->GetUniqueID()));
			m_immutableAllocations.m_handleToPtr[pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			SEAssert("Parameter block is already registered",
				!m_mutableAllocations.m_handleToPtr.contains(pb->GetUniqueID()));

			m_mutableAllocations.m_handleToPtr[pb->GetUniqueID()] = pb;
		}
		break;
		default:
		{
			SEAssertF("Invalid update type");
		}
		}

		// Pre-allocate our PB so it's ready to commit to:
		Allocate(pb->GetUniqueID(), numBytes, pbType);
	}


	void ParameterBlockAllocator::Allocate(Handle uniqueID, uint32_t numBytes, ParameterBlock::PBType pbType)
	{
		SEAssert("Permanent parameter blocks can only be allocated at startup, before the 1st render frame",
			pbType == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded);

		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			SEAssert("A parameter block with this handle has already been added",
				m_uniqueIDToTypeAndByteIndex.find(uniqueID) == m_uniqueIDToTypeAndByteIndex.end());
		}

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		uint32_t dataIndex;
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			const size_t writeIdx = GetWriteIdx();

			SEAssert("Allocation will be out of bounds. Increase the fixed allocation size",
				m_singleFrameAllocations.m_baseIdx[writeIdx] + numBytes <= k_fixedAllocationByteSize);

			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			dataIndex = m_singleFrameAllocations.m_baseIdx[writeIdx];
			m_singleFrameAllocations.m_baseIdx[writeIdx] += numBytes;
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);

			dataIndex = util::CheckedCast<uint32_t>(m_immutableAllocations.m_committed.size());
			m_immutableAllocations.m_committed.resize(m_immutableAllocations.m_committed.size() + numBytes, 0);
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			SEAssert("Allocations are out of sync",
				m_mutableAllocations.m_committed[0].size() == m_mutableAllocations.m_committed[1].size());

			dataIndex = util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed[0].size());

			const uint32_t resizeAmt = util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed[0].size() + numBytes);
			m_mutableAllocations.m_committed[0].resize(resizeAmt, 0);
			m_mutableAllocations.m_committed[1].resize(resizeAmt, 0);
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
			dataIndex = -1; // Make our insertion index obviously incorrect
		}
		}

		// Update our ID -> data tracking table:
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);
			m_uniqueIDToTypeAndByteIndex.insert({ uniqueID, {pbType, dataIndex, numBytes} });
		}
	}


	void ParameterBlockAllocator::Commit(Handle uniqueID, void const* data)
	{
		uint32_t startIdx;
		uint32_t numBytes;
		ParameterBlock::PBType pbType;
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& result = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

			SEAssert("Parameter block with this ID has not been allocated",
				result != m_uniqueIDToTypeAndByteIndex.end());

			SEAssert("Immutable parameter blocks can only be committed at startup",
				!m_allocationPeriodEnded || result->second.m_type != ParameterBlock::PBType::Immutable);

			startIdx = result->second.m_startIndex;
			numBytes = result->second.m_numBytes;
			pbType = result->second.m_type;
		}

		// Copy the data to our pre-allocated region.
		// Note: We still need to lock our mutexes before copying, incase the vectors are resized by another allocation
		void* dest = nullptr;
		const size_t writeIdx = GetWriteIdx();
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
			dest = &m_singleFrameAllocations.m_committed[writeIdx][startIdx];
			memcpy(dest, data, numBytes);
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			dest = &m_immutableAllocations.m_committed[startIdx];
			memcpy(dest, data, numBytes);
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
			dest = &m_mutableAllocations.m_committed[writeIdx][startIdx];
			memcpy(dest, data, numBytes);
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
		}
		}

		// If this is the 1st commit, we need to also copy the data to the other side of the mutables double buffer,
		// incase it doesn't get a an update the next frame
		if (!m_allocationPeriodEnded && pbType == ParameterBlock::PBType::Mutable)
		{
			const size_t readIdx = GetReadIdx();

			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);			
			dest = &m_mutableAllocations.m_committed[readIdx][startIdx];
			memcpy(dest, data, numBytes);
		}

		// Add the committed PB to our dirty list, so we can buffer the data when required
		{
			std::lock_guard<std::mutex> dirtyLock(m_dirtyParameterBlocksMutex);
			m_dirtyParameterBlocks[writeIdx].emplace(uniqueID);
		}
	}


	void ParameterBlockAllocator::GetDataAndSize(Handle uniqueID, void const*& out_data, uint32_t& out_numBytes) const
	{
		ParameterBlock::PBType pbType;
		uint32_t startIdx;
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& result = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

			SEAssert("Parameter block with this ID has not been allocated",
				result != m_uniqueIDToTypeAndByteIndex.end());

			out_numBytes = result->second.m_numBytes;

			pbType = result->second.m_type;
			startIdx = result->second.m_startIndex;
		}

		const size_t readIdx = GetReadIdx(); // This function is typically called when binding PB data to shaders

		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
			out_data = &m_singleFrameAllocations.m_committed[readIdx][startIdx];
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			out_data = &m_immutableAllocations.m_committed[startIdx];
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
			out_data = &m_mutableAllocations.m_committed[readIdx][startIdx];
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
		}
		}
	}


	uint32_t ParameterBlockAllocator::GetSize(Handle uniqueID) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

		auto const& result = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Parameter block with this ID has not been allocated",
			result != m_uniqueIDToTypeAndByteIndex.end());

		return result->second.m_numBytes;
	}


	void ParameterBlockAllocator::Deallocate(Handle uniqueID)
	{
		ParameterBlock::PBType pbType;
		uint32_t startIdx;
		uint32_t numBytes;
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& pb = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

			SEAssert("Cannot deallocate a parameter block that does not exist", pb != m_uniqueIDToTypeAndByteIndex.end());

			pbType = pb->second.m_type;
			startIdx = pb->second.m_startIndex;
			numBytes = pb->second.m_numBytes;
		}

		const uint64_t currentRenderFrameNum = re::RenderManager::Get()->GetCurrentRenderFrameNum();

		switch (pbType)
		{
		case ParameterBlock::PBType::Immutable: // Should only deallocate at shutdown
		{
			AddToDeferredDeletions(currentRenderFrameNum, m_immutableAllocations.m_handleToPtr.at(uniqueID));

			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			m_immutableAllocations.m_handleToPtr.erase(uniqueID);
		}
		break;
		case ParameterBlock::PBType::Mutable: // Should only deallocate at shutdown
		{
			AddToDeferredDeletions(currentRenderFrameNum, m_mutableAllocations.m_handleToPtr.at(uniqueID));

			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
			m_mutableAllocations.m_handleToPtr.erase(uniqueID);
		}
		break;		
		case ParameterBlock::PBType::SingleFrame:
		{
			// Deallocation uses the read index, as PBs are typically destroyed right before we clear them
			const size_t readIdx = GetReadIdx();
			AddToDeferredDeletions(currentRenderFrameNum, m_singleFrameAllocations.m_handleToPtr[readIdx].at(uniqueID));

			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
			m_singleFrameAllocations.m_handleToPtr[readIdx].erase(uniqueID);
		}
		break;
		default:
			SEAssertF("Invalid parameter block type");
		}

		// Remove the ID from our map
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& pb = m_uniqueIDToTypeAndByteIndex.find(uniqueID);
			m_uniqueIDToTypeAndByteIndex.erase(pb);
		}
	}


	// Buffer dirty PB data
	void ParameterBlockAllocator::BufferParamBlocks()
	{
		SEBeginCPUEvent("re::ParameterBlockAllocator::BufferParamBlocks");

		SEAssert("Cannot buffer param blocks until they're all allocated", m_allocationPeriodEnded);

		const size_t readIdx = GetReadIdx();

		std::lock_guard<std::mutex> dirtyLock(m_dirtyParameterBlocksMutex);

		while (!m_dirtyParameterBlocks[readIdx].empty())
		{
			const Handle currentHandle = m_dirtyParameterBlocks[readIdx].front();

			ParameterBlock::PBType type = ParameterBlock::PBType::PBType_Count;
			{
				std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);
				type = m_uniqueIDToTypeAndByteIndex.find(currentHandle)->second.m_type;
			}

			re::ParameterBlock* currentPB = nullptr;
			switch (type)
			{
			case ParameterBlock::PBType::Mutable:
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				currentPB = m_mutableAllocations.m_handleToPtr[currentHandle].get();
			}
			break;
			case ParameterBlock::PBType::Immutable:
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				currentPB = m_immutableAllocations.m_handleToPtr[currentHandle].get();
			}
			break;
			case ParameterBlock::PBType::SingleFrame:
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				currentPB = m_singleFrameAllocations.m_handleToPtr[readIdx][currentHandle].get();
			}
			break;
			default:
				SEAssertF("Invalid PBType");
			}

			platform::ParameterBlock::Update(*currentPB);

			m_dirtyParameterBlocks[readIdx].pop();
		}

		SEEndCPUEvent();
	}


	void ParameterBlockAllocator::SwapPlatformBuffers(uint64_t renderFrameNum)
	{
		m_platformParams->BeginFrame();
	}


	void ParameterBlockAllocator::SwapCPUBuffers(uint64_t renderFrameNum)
	{
		m_readFrameNum = renderFrameNum;
	}


	void ParameterBlockAllocator::EndFrame()
	{
		SEBeginCPUEvent("re::ParameterBlockAllocator::EndFrame");

		// Clear single-frame allocations:
		{
			const size_t readIdx = GetReadIdx();

			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			for (auto const& it : m_singleFrameAllocations.m_handleToPtr[readIdx])
			{
				SEAssert("Trying to deallocate a single frame parameter block, but there is still a live shared_ptr. Is "
					"something holding onto a single frame parameter block beyond the frame lifetime?",
					it.second.use_count() == 1);

				const uint64_t currentRenderFrameNum = re::RenderManager::Get()->GetCurrentRenderFrameNum();
				AddToDeferredDeletions(currentRenderFrameNum, it.second);
			}

			// Debug: Track the high-water mark for the max single-frame PB allocations
			m_maxSingleFrameAllocations = 
				std::max(m_maxSingleFrameAllocations, util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_handleToPtr[readIdx].size()));
			m_maxSingleFrameAllocationByteSize = 
				std::max(m_maxSingleFrameAllocationByteSize, m_singleFrameAllocations.m_baseIdx[readIdx]);

			m_singleFrameAllocations.m_handleToPtr[readIdx].clear();
			m_singleFrameAllocations.m_baseIdx[readIdx] = 0;
		}

		ClearDeferredDeletions(m_readFrameNum);

		SEEndCPUEvent();
	}


	void ParameterBlockAllocator::ClearDeferredDeletions(uint64_t frameNum)
	{
		SEAssert("Trying to clear before the first swap buffer call", 
			m_readFrameNum != std::numeric_limits<uint64_t>::max());

		SEBeginCPUEvent(
			std::format("ParameterBlockAllocator::ClearDeferredDeletions ({})", m_deferredDeleteQueue.size()).c_str());

		std::lock_guard<std::mutex> lock (m_deferredDeleteQueueMutex);

		while (!m_deferredDeleteQueue.empty() && m_deferredDeleteQueue.front().first + k_deferredDeleteNumFrames < frameNum)
		{
			platform::ParameterBlock::Destroy(*m_deferredDeleteQueue.front().second);
			m_deferredDeleteQueue.pop();
		}

		SEEndCPUEvent();
	}


	void ParameterBlockAllocator::AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::ParameterBlock> pb)
	{
		std::lock_guard<std::mutex> lock(m_deferredDeleteQueueMutex);

		m_deferredDeleteQueue.emplace(std::pair<uint64_t, std::shared_ptr<re::ParameterBlock>>{frameNum, pb});
	}
}