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
		, m_allocationPeriodEnded(false)
		, m_permanentPBsHaveBeenBuffered(false)
		, m_isValid(true)
		, m_maxSingleFrameAllocations(0) // Debug: Track the high-water mark for the max single-frame PB allocations
		, m_maxSingleFrameAllocationByteSize(0)
	{
		for (uint8_t pbType = 0; pbType < re::ParameterBlock::PBType::PBType_Count; pbType++)
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);
				m_allocations[pbType].m_committed.reserve(k_systemMemoryReservationSize);
			}
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
				m_allocations[re::ParameterBlock::PBType::Mutable].m_mutex,
				m_allocations[re::ParameterBlock::PBType::Immutable].m_mutex,
				m_allocations[re::ParameterBlock::PBType::SingleFrame].m_mutex);
			static_assert(re::ParameterBlock::PBType::PBType_Count == 3);

			LOG(std::format("ParameterBlockAllocator shutting down. Session usage statistics:\n"
				"\t{} Immutable permanent allocations: {} B\n"
				"\t{} Mutable permanent allocations: {} B\n"
				"\t{} max single-frame allocations, max {} B buffer usage seen",
				m_allocations[re::ParameterBlock::PBType::Immutable].m_handleToPtr.size(),
				m_allocations[re::ParameterBlock::PBType::Immutable].m_committed.size(),
				m_allocations[re::ParameterBlock::PBType::Mutable].m_handleToPtr.size(),
				m_allocations[re::ParameterBlock::PBType::Mutable].m_committed.size(),
				m_maxSingleFrameAllocations,
				m_maxSingleFrameAllocationByteSize).c_str());

			// Must clear the parameter blocks shared_ptrs before clearing the committed memory
			for (Allocation& allocation : m_allocations)
			{
				// Destroy() removes the PB from our unordered_map & invalidates iterators; Just loop until it's empty
				while (!allocation.m_handleToPtr.empty())
				{
					allocation.m_handleToPtr.begin()->second->Destroy();
				}
				SEAssert(allocation.m_handleToPtr.empty(), "Failed to clear the map");

				allocation.m_committed.clear();
			}

			// Clear the handle -> commit map
			m_handleToTypeAndByteIndex.clear();
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


	void ParameterBlockAllocator::ClosePermanentPBRegistrationPeriod()
	{
		m_allocationPeriodEnded = true;
	}


	void ParameterBlockAllocator::RegisterAndAllocateParameterBlock(
		std::shared_ptr<re::ParameterBlock> pb, uint32_t numBytes)
	{
		SEAssert(pb->GetType() == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded,
			"Permanent parameter blocks can only be registered at startup, before the 1st render frame");

		const ParameterBlock::PBType pbType = pb->GetType();
		SEAssert(pbType != re::ParameterBlock::PBType::PBType_Count, "Invalid PBType");

		const Handle uniqueID = pb->GetUniqueID();
		{
			std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);

			SEAssert(!m_allocations[pbType].m_handleToPtr.contains(uniqueID),
				"Parameter block is already registered");

			m_allocations[pbType].m_handleToPtr[uniqueID] = pb;
		}

		// Pre-allocate our PB so it's ready to commit to:
		Allocate(uniqueID, numBytes, pbType);
	}


	void ParameterBlockAllocator::Allocate(Handle uniqueID, uint32_t numBytes, ParameterBlock::PBType pbType)
	{
		SEAssert(pbType == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded,
			"Permanent parameter blocks can only be allocated at startup, before the 1st render frame");

		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			SEAssert(m_handleToTypeAndByteIndex.find(uniqueID) == m_handleToTypeAndByteIndex.end(),
				"A parameter block with this handle has already been added");
		}

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		uint32_t dataIndex = -1; // Start with something obviously incorrect

		{
			std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);

			dataIndex = util::CheckedCast<uint32_t>(m_allocations[pbType].m_committed.size());

			const uint32_t resizeAmt = util::CheckedCast<uint32_t>(m_allocations[pbType].m_committed.size() + numBytes);
			m_allocations[pbType].m_committed.resize(resizeAmt, 0);
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

			SEAssert(!m_allocationPeriodEnded || result->second.m_type != ParameterBlock::PBType::Immutable,
				"Immutable parameter blocks can only be committed at startup");

			startIdx = result->second.m_startIndex;
			numBytes = result->second.m_numBytes;
			pbType = result->second.m_type;
		}

		// Copy the data to our pre-allocated region.
		// Note: We still need to lock our mutexes before copying, incase the vectors are resized by another allocation
		void* dest = nullptr;
		{
			std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);

			dest = &m_allocations[pbType].m_committed[startIdx];
			memcpy(dest, data, numBytes);
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
		{
			std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);
			out_data = &m_allocations[pbType].m_committed[startIdx];
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

		AddToDeferredDeletions(m_currentFrameNum, m_allocations[pbType].m_handleToPtr.at(uniqueID));

		// Erase the PB from our allocations:
		{
			std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);
			m_allocations[pbType].m_handleToPtr.erase(uniqueID);
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
	}


	// Buffer dirty PB data
	void ParameterBlockAllocator::BufferParamBlocks()
	{
		SEAssert(m_allocationPeriodEnded, "Cannot buffer param blocks until they're all allocated");

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
				{
					std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);
					currentPB = m_allocations[pbType].m_handleToPtr[currentHandle].get();
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
			std::lock_guard<std::recursive_mutex> lock(m_allocations[re::ParameterBlock::PBType::SingleFrame].m_mutex);

			// Debug: Track the high-water mark for the max single-frame PB allocations
			m_maxSingleFrameAllocations = std::max(
				m_maxSingleFrameAllocations,
				util::CheckedCast<uint32_t>(m_allocations[re::ParameterBlock::PBType::SingleFrame].m_handleToPtr.size()));
			m_maxSingleFrameAllocationByteSize = std::max(
				m_maxSingleFrameAllocationByteSize,
				util::CheckedCast<uint32_t>(m_allocations[re::ParameterBlock::PBType::SingleFrame].m_committed.size()));

			// Calling Destroy() on our ParameterBlock recursively calls ParameterBlockAllocator::Deallocate, which
			// erases an entry from m_singleFrameAllocations.m_handleToPtr. Thus, we can't use an iterator as it'll be
			// invalidated. Instead, we just loop until it's empty
			while (!m_allocations[re::ParameterBlock::PBType::SingleFrame].m_handleToPtr.empty())
			{
				SEAssert(
					m_allocations[re::ParameterBlock::PBType::SingleFrame].m_handleToPtr.begin()->second.use_count() == 1,
					"Trying to deallocate a single frame parameter block, but there is still a live shared_ptr. Is "
					"something holding onto a single frame parameter block beyond the frame lifetime?");

				m_allocations[re::ParameterBlock::PBType::SingleFrame].m_handleToPtr.begin()->second->Destroy();
			}

			m_allocations[re::ParameterBlock::PBType::SingleFrame].m_handleToPtr.clear();
			m_allocations[re::ParameterBlock::PBType::SingleFrame].m_committed.clear();
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