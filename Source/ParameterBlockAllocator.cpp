// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "CastUtils.h"
#include "ParameterBlockAllocator.h"
#include "ParameterBlockAllocator_Platform.h"
#include "ParameterBlock_Platform.h"
#include "ProfilingMarkers.h"
#include "RenderManager.h"
#include "RenderManager_Platform.h"


namespace
{
	constexpr uint64_t k_invalidFrameNum = std::numeric_limits<uint64_t>::max();
}

namespace re
{
	// Parameter Block Platform Params:
	//---------------------------------

	ParameterBlockAllocator::PlatformParams::PlatformParams()
		: m_numBuffers(platform::RenderManager::GetNumFramesInFlight())
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

		SEAssert(allocationBaseIdx + alignedSize <= k_fixedAllocationByteSize,
			"Allocation is out of bounds. Consider increasing k_fixedAllocationByteSize");

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
		: m_numFramesInFlight(3) // Safe default: We'll fetch the correct value during Create()
		, m_currentFrameNum(k_invalidFrameNum)
		, m_isValid(true)
		, m_maxSingleFrameAllocations(0) // Debug: Track the high-water mark for the max single-frame PB allocations
		, m_maxSingleFrameAllocationByteSize(0)
	{
		// Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
			m_mutableAllocations.m_committed.reserve(k_permanentReservationCount);
		}
		// Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			m_immutableAllocations.m_committed.reserve(k_permanentReservationCount);
		}
		// Single frame:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
			m_singleFrameAllocations.m_committed.reserve(k_singleFrameReservationBytes);
		}

		platform::ParameterBlockAllocator::CreatePlatformParams(*this);

		constexpr uint32_t k_expectedMutablePBCount = 64; // Arbitrary
		m_dirtyMutablePBFrameNum.reserve(k_expectedMutablePBCount);
	}


	void ParameterBlockAllocator::Create(uint64_t currentFrame)
	{
		m_currentFrameNum = currentFrame;
		platform::ParameterBlockAllocator::Create(*this);

		m_numFramesInFlight = re::RenderManager::GetNumFramesInFlight();
	}


	ParameterBlockAllocator::~ParameterBlockAllocator()
	{
		SEAssert(!IsValid(),
			"Parameter block allocator destructor called before Destroy(). The parameter block allocator must "
			"be manually destroyed (i.e. in the api-specific Context::Destroy())");
	}
	

	void ParameterBlockAllocator::Destroy()
	{
		{
			std::scoped_lock lock(
				m_handleToTypeAndByteIndexMutex,
				m_mutableAllocations.m_mutex,
				m_immutableAllocations.m_mutex,
				m_singleFrameAllocations.m_mutex);

			// Sum the number of bytes used by our permanent PBs:
			size_t numMutablePBBytes = 0;
			for (size_t i = 0; i < m_mutableAllocations.m_committed.size(); i++)
			{
				numMutablePBBytes += m_mutableAllocations.m_committed[i].size();
			}
			size_t numImmutablePBBytes = 0;
			for (size_t i = 0; i < m_immutableAllocations.m_committed.size(); i++)
			{
				numImmutablePBBytes += m_immutableAllocations.m_committed[i].size();
			}

			LOG(std::format("ParameterBlockAllocator shutting down. Session usage statistics:\n"
				"\t{} Immutable permanent allocations: {} B\n"
				"\t{} Mutable permanent allocations: {} B\n"
				"\t{} max single-frame allocations, max {} B buffer usage seen",
				m_immutableAllocations.m_handleToPtr.size(),
				numImmutablePBBytes,
				m_mutableAllocations.m_handleToPtr.size(),
				numMutablePBBytes,
				m_maxSingleFrameAllocations,
				m_maxSingleFrameAllocationByteSize).c_str());

			// Must clear the parameter blocks shared_ptrs before clearing the committed memory
			// Destroy() removes the PB from our unordered_map & invalidates iterators; Just loop until it's empty
			auto ClearPBPointers = [](std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>>& handleToPtr)
				{
					while (!handleToPtr.empty())
					{
						handleToPtr.begin()->second->Destroy();
					}
					SEAssert(handleToPtr.empty(), "Failed to clear the map");
				};
			ClearPBPointers(m_mutableAllocations.m_handleToPtr);
			ClearPBPointers(m_immutableAllocations.m_handleToPtr);
			ClearPBPointers(m_singleFrameAllocations.m_handleToPtr);

			SEAssert(m_mutableAllocations.m_committed.empty(), "Mutable committed data should be cleared by now");
			SEAssert(m_immutableAllocations.m_committed.empty(), "Immutable committed data should be cleared by now");
			SEAssert(m_singleFrameAllocations.m_committed.empty(), "Single frame committed data should be cleared by now");

			SEAssert(m_handleToTypeAndByteIndex.empty(), "Handle to type and byte map should be cleared by now");
		}

		std::queue<Handle> emptyQueue;
		m_dirtyParameterBlocks.swap(emptyQueue);

		// The platform::RenderManager has already flushed all outstanding work; Force our deferred deletions to be
		// immediately cleared
		constexpr uint64_t k_maxFrameNum = std::numeric_limits<uint64_t>::max();
		ClearDeferredDeletions(k_maxFrameNum);

		platform::ParameterBlockAllocator::Destroy(*this);

		m_isValid = false;
	}


	bool ParameterBlockAllocator::IsValid() const
	{
		return m_isValid;
	}


	void ParameterBlockAllocator::RegisterAndAllocateParameterBlock(
		std::shared_ptr<re::ParameterBlock> pb, uint32_t numBytes)
	{
		const ParameterBlock::PBType pbType = pb->GetType();
		SEAssert(pbType != re::ParameterBlock::PBType::PBType_Count, "Invalid PBType");

		const Handle uniqueID = pb->GetUniqueID();

		auto RecordHandleToPointer = [&](
			std::recursive_mutex& mutex,
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>>& handleToPtr)
			{
				std::lock_guard<std::recursive_mutex> lock(mutex);
				SEAssert(!handleToPtr.contains(uniqueID), "Parameter block is already registered");
				handleToPtr[uniqueID] = pb;
			};

		switch (pbType)
		{
		case ParameterBlock::PBType::Mutable:
		{
			RecordHandleToPointer(m_mutableAllocations.m_mutex, m_mutableAllocations.m_handleToPtr);
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			RecordHandleToPointer(m_immutableAllocations.m_mutex, m_immutableAllocations.m_handleToPtr);
		}
		break;
		case ParameterBlock::PBType::SingleFrame:
		{
			RecordHandleToPointer(m_singleFrameAllocations.m_mutex, m_singleFrameAllocations.m_handleToPtr);
		}
		break;
		default: SEAssertF("Invalid PBType");
		}

		// Pre-allocate our PB so it's ready to commit to:
		Allocate(uniqueID, numBytes, pbType);
	}


	void ParameterBlockAllocator::Allocate(Handle uniqueID, uint32_t numBytes, ParameterBlock::PBType pbType)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			SEAssert(m_handleToTypeAndByteIndex.find(uniqueID) == m_handleToTypeAndByteIndex.end(),
				"A parameter block with this handle has already been added");
		}

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		uint32_t dataIndex = -1; // Start with something obviously incorrect

		switch (pbType)
		{
		case ParameterBlock::PBType::Mutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				dataIndex = util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed.size());
				m_mutableAllocations.m_committed.emplace_back(numBytes, 0);
			}
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			dataIndex = util::CheckedCast<uint32_t>(m_immutableAllocations.m_committed.size());
			m_immutableAllocations.m_committed.emplace_back(numBytes, 0);
		}
		break;
		case ParameterBlock::PBType::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

				dataIndex = util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size());

				const uint32_t resizeAmt = 
					util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size() + numBytes);

				m_singleFrameAllocations.m_committed.resize(resizeAmt, 0);
			}
		}
		break;
		default: SEAssertF("Invalid PBType");
		}

		// Update our ID -> data tracking table:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);
			m_handleToTypeAndByteIndex.insert({ uniqueID, {pbType, dataIndex, numBytes} });
		}
	}


	void ParameterBlockAllocator::Commit(Handle uniqueID, void const* data)
	{
		uint32_t startIdx;
		uint32_t numBytes;
		ParameterBlock::PBType pbType;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);

			SEAssert(result != m_handleToTypeAndByteIndex.end(),
				"Parameter block with this ID has not been allocated");

			startIdx = result->second.m_startIndex;
			numBytes = result->second.m_numBytes;
			pbType = result->second.m_type;
		}


		// Copy the data to our pre-allocated region.
		void* dest = nullptr;
		switch (pbType)
		{
		case ParameterBlock::PBType::Mutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				SEAssert(m_mutableAllocations.m_committed[startIdx].size() == numBytes, "Size mismatch");
				dest = m_mutableAllocations.m_committed[startIdx].data();
				memcpy(dest, data, numBytes);
			}
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				SEAssert(m_immutableAllocations.m_committed[startIdx].size() == numBytes, "Size mismatch");
				dest = m_immutableAllocations.m_committed[startIdx].data();
				memcpy(dest, data, numBytes);
			}
		}
		break;
		case ParameterBlock::PBType::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				dest = &m_singleFrameAllocations.m_committed[startIdx];
				memcpy(dest, data, numBytes);
			}
		}
		break;
		default: SEAssertF("Invalid PBType");
		}

		// Add the committed PB to our dirty list, so we can buffer the data when required
		{
			std::lock_guard<std::mutex> lock(m_dirtyParameterBlocksMutex);
			m_dirtyParameterBlocks.emplace(uniqueID);
		}

		// Update mutable parameter blocks modification frame
		if (pbType == ParameterBlock::PBType::Mutable)
		{
			{
				std::lock_guard<std::mutex> lock(m_dirtyParameterBlocksMutex);				
				m_dirtyMutablePBFrameNum[uniqueID] = m_currentFrameNum; // Insert/update
			}
		}
	}


	void ParameterBlockAllocator::GetDataAndSize(Handle uniqueID, void const*& out_data, uint32_t& out_numBytes) const
	{
		ParameterBlock::PBType pbType;
		uint32_t startIdx = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);
			SEAssert(result != m_handleToTypeAndByteIndex.end(), "Parameter block with this ID has not been allocated");

			pbType = result->second.m_type;
			startIdx = result->second.m_startIndex;

			out_numBytes = result->second.m_numBytes;
		}

		// Note: This is not thread safe, as the pointer will become stale if m_committed is resized. This should be
		// fine though, as the ParameterBlockAllocator is simply a temporary staging ground for data about to be copied
		// to GPU heaps. Copies in/resizing should all be done before this function is ever called
		switch (pbType)
		{
		case ParameterBlock::PBType::Mutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				out_data = m_mutableAllocations.m_committed[startIdx].data();
			}
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				out_data = m_immutableAllocations.m_committed[startIdx].data();
			}
		}
		break;
		case ParameterBlock::PBType::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				out_data = &m_singleFrameAllocations.m_committed[startIdx];
			}
		}
		break;
		default: SEAssertF("Invalid PBType");
		}
	}


	uint32_t ParameterBlockAllocator::GetSize(Handle uniqueID) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

		auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);

		SEAssert(result != m_handleToTypeAndByteIndex.end(), "Parameter block with this ID has not been allocated");

		return result->second.m_numBytes;
	}


	void ParameterBlockAllocator::Deallocate(Handle uniqueID)
	{
		ParameterBlock::PBType pbType = re::ParameterBlock::PBType::PBType_Count;
		uint32_t startIdx = -1;
		uint32_t numBytes = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& pb = m_handleToTypeAndByteIndex.find(uniqueID);
			SEAssert(pb != m_handleToTypeAndByteIndex.end(), "Cannot deallocate a parameter block that does not exist");

			pbType = pb->second.m_type;
			startIdx = pb->second.m_startIndex;
			numBytes = pb->second.m_numBytes;
		}

		// Add our PB to the deferred deletion queue, then erase the pointer from our allocation list
		auto ProcessErasure = [&](
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>>& handleToPtr,
			std::recursive_mutex& mutex)
			{
				AddToDeferredDeletions(m_currentFrameNum, handleToPtr.at(uniqueID));

				// Erase the PB from our allocations:
				{
					std::lock_guard<std::recursive_mutex> lock(mutex);
					handleToPtr.erase(uniqueID);
				}
			};
		switch (pbType)
		{
		case ParameterBlock::PBType::Mutable:
		{
			ProcessErasure(m_mutableAllocations.m_handleToPtr, m_mutableAllocations.m_mutex);
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			ProcessErasure(m_immutableAllocations.m_handleToPtr, m_immutableAllocations.m_mutex);
		}
		break;
		case ParameterBlock::PBType::SingleFrame:
		{
			ProcessErasure(m_singleFrameAllocations.m_handleToPtr, m_singleFrameAllocations.m_mutex);
		}
		break;
		default: SEAssertF("Invalid PBType");
		}


		// Remove the handle from our map:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& pb = m_handleToTypeAndByteIndex.find(uniqueID);
			m_handleToTypeAndByteIndex.erase(pb);
		}

		if (pbType == ParameterBlock::PBType::Mutable)
		{
			{
				std::lock_guard<std::mutex> lock(m_dirtyParameterBlocksMutex);
				m_dirtyMutablePBFrameNum.erase(uniqueID);
			}
		}

		// Finally, free the committed memory:
		auto FreePermanentCommit = [&](
			std::recursive_mutex& mutex,
			std::vector<std::vector<uint8_t>>& committed)
			{
				{
					std::scoped_lock lock(mutex, m_handleToTypeAndByteIndexMutex);

					const size_t idxToReplace = startIdx;
					const size_t idxToMove = committed.size() - 1;

					if (idxToReplace != idxToMove)
					{
						committed[idxToReplace] = std::move(committed[idxToMove]);

						// Update the records for the entry that we moved. This is a slow linear search through an
						// unordered map, but permanent parameter blocks should be deallocated very infrequently
						for (auto& entry : m_handleToTypeAndByteIndex)
						{
							if (entry.second.m_startIndex == idxToMove)
							{
								entry.second.m_startIndex = util::CheckedCast<uint32_t>(idxToReplace);
								break;
							}
						}
					}
					committed.pop_back();
				}
			};
		switch (pbType)
		{
		case ParameterBlock::PBType::Mutable:
		{
			FreePermanentCommit(m_mutableAllocations.m_mutex, m_mutableAllocations.m_committed);
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			FreePermanentCommit(m_immutableAllocations.m_mutex, m_immutableAllocations.m_committed);
		}
		break;
		case ParameterBlock::PBType::SingleFrame:
		{
			// Single frame PB memory is already cleared at the end of every frame
		}
		break;
		default: SEAssertF("Invalid PBType");
		}
	}


	// Buffer dirty PB data
	void ParameterBlockAllocator::BufferParamBlocks()
	{
		SEBeginCPUEvent("re::ParameterBlockAllocator::BufferParamBlocks");
		{
			std::lock_guard<std::mutex> dirtyLock(m_dirtyParameterBlocksMutex);

			// We keep mutable parameter blocks committed within m_numFramesInFlight in the dirty list to ensure they're
			// kept up to date
			std::queue<Handle> dirtyMutablePBs;

			// Prevent duplicated updates for mutable param blocks by tracking if we've already buffered them
			static size_t s_prevBufferedPBCount = 32;
			std::unordered_set<Handle> bufferedMutablePBs;
			bufferedMutablePBs.reserve(s_prevBufferedPBCount);

			const uint8_t heapOffsetFactor = m_currentFrameNum % m_numFramesInFlight;

			while (!m_dirtyParameterBlocks.empty())
			{
				const Handle currentHandle = m_dirtyParameterBlocks.front();
				m_dirtyParameterBlocks.pop();

				ParameterBlock::PBType pbType = ParameterBlock::PBType::PBType_Count;
				{
					std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);
					pbType = m_handleToTypeAndByteIndex.find(currentHandle)->second.m_type;
				}

				re::ParameterBlock const* currentPB = nullptr;

				auto GetCurrentPBPtr = [&](
					std::recursive_mutex& mutex, 
					std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> const& handleToPtr)
					{
						std::lock_guard<std::recursive_mutex> lock(mutex);
						SEAssert(handleToPtr.contains(currentHandle), "PB is not registered");
						currentPB = handleToPtr.at(currentHandle).get();
					};
				switch (pbType)
				{
				case ParameterBlock::PBType::Mutable:
				{
					GetCurrentPBPtr(m_mutableAllocations.m_mutex, m_mutableAllocations.m_handleToPtr);
				}
				break;
				case ParameterBlock::PBType::Immutable:
				{
					GetCurrentPBPtr(m_immutableAllocations.m_mutex, m_immutableAllocations.m_handleToPtr);
				}
				break;
				case ParameterBlock::PBType::SingleFrame:
				{
					GetCurrentPBPtr(m_singleFrameAllocations.m_mutex, m_singleFrameAllocations.m_handleToPtr);
				}
				break;
				default: SEAssertF("Invalid PBType");
				}

				if (pbType != ParameterBlock::PBType::Mutable || 
					!bufferedMutablePBs.contains(currentHandle)) // Only buffer mutable PBs once per frame
				{
					platform::ParameterBlock::Update(*currentPB, heapOffsetFactor);
				}

				// Update our tracking of mutable parameter blocks:
				if (pbType == ParameterBlock::PBType::Mutable)
				{
					SEAssert(m_dirtyMutablePBFrameNum.contains(currentHandle),
						"Cannot find mutable parameter block, was it ever committed?");

					// If this is the first time we've seen a mutable parameter block while buffering, and its been
					// updated within the m_numFramesInFlight, add it back to the dirty list for the next frame
					if (!bufferedMutablePBs.contains(currentHandle) &&
						m_dirtyMutablePBFrameNum.at(currentHandle) + m_numFramesInFlight > m_currentFrameNum)
					{
						dirtyMutablePBs.emplace(currentHandle);
						bufferedMutablePBs.emplace(currentHandle);
					}
				}
			}

			// Swap in our dirty queue for the next frame
			if (!dirtyMutablePBs.empty())
			{
				m_dirtyParameterBlocks = std::move(dirtyMutablePBs);
			}

			s_prevBufferedPBCount = std::max(s_prevBufferedPBCount, bufferedMutablePBs.size());
		}

		SEEndCPUEvent();
	}


	void ParameterBlockAllocator::BeginFrame(uint64_t renderFrameNum)
	{
		// Avoid stomping existing data when the ParameterBlockAllocator has already been accessed (e.g. during
		// RenderManager::Initialize, before ParameterBlockAllocator::BeginFrame has been called)
		if (renderFrameNum != m_currentFrameNum)
		{
			m_currentFrameNum = renderFrameNum;
			m_platformParams->BeginFrame();
		}
	}


	void ParameterBlockAllocator::EndFrame()
	{
		SEBeginCPUEvent("re::ParameterBlockAllocator::EndFrame");

		// Clear single-frame allocations:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			// Debug: Track the high-water mark for the max single-frame PB allocations
			m_maxSingleFrameAllocations = std::max(
				m_maxSingleFrameAllocations,
				util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_handleToPtr.size()));
			m_maxSingleFrameAllocationByteSize = std::max(
				m_maxSingleFrameAllocationByteSize,
				util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size()));

			// Calling Destroy() on our ParameterBlock recursively calls ParameterBlockAllocator::Deallocate, which
			// erases an entry from m_singleFrameAllocations.m_handleToPtr. Thus, we can't use an iterator as it'll be
			// invalidated. Instead, we just loop until it's empty
			while (!m_singleFrameAllocations.m_handleToPtr.empty())
			{
				SEAssert(m_singleFrameAllocations.m_handleToPtr.begin()->second.use_count() == 1,
					"Trying to deallocate a single frame parameter block, but there is still a live shared_ptr. Is "
					"something holding onto a single frame parameter block beyond the frame lifetime?");

				m_singleFrameAllocations.m_handleToPtr.begin()->second->Destroy();
			}

			m_singleFrameAllocations.m_handleToPtr.clear();
			m_singleFrameAllocations.m_committed.clear();
		}

		ClearDeferredDeletions(m_currentFrameNum);

		SEEndCPUEvent();
	}


	void ParameterBlockAllocator::ClearDeferredDeletions(uint64_t frameNum)
	{
		SEAssert(m_currentFrameNum != std::numeric_limits<uint64_t>::max(),
			"Trying to clear before the first swap buffer call");

		SEBeginCPUEvent(
			std::format("ParameterBlockAllocator::ClearDeferredDeletions ({})", m_deferredDeleteQueue.size()).c_str());

		{
			std::lock_guard<std::mutex> lock(m_deferredDeleteQueueMutex);

			while (!m_deferredDeleteQueue.empty() &&
				m_deferredDeleteQueue.front().first + m_numFramesInFlight < frameNum)
			{
				platform::ParameterBlock::Destroy(*m_deferredDeleteQueue.front().second);
				m_deferredDeleteQueue.pop();
			}
		}

		SEEndCPUEvent();
	}


	void ParameterBlockAllocator::AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::ParameterBlock> pb)
	{
		std::lock_guard<std::mutex> lock(m_deferredDeleteQueueMutex);

		m_deferredDeleteQueue.emplace(std::pair<uint64_t, std::shared_ptr<re::ParameterBlock>>{frameNum, pb});
	}
}