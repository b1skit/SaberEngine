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

		SEAssert("Allocation is out of bounds. Consider increasing k_fixedAllocationByteSize", 
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
		: m_maxSingleFrameAllocations(0) // Debug: Track the high-water mark for the max single-frame PB allocations
		, m_maxSingleFrameAllocationByteSize(0)
		, m_allocationPeriodEnded(false)
		, m_permanentPBsHaveBeenBuffered(false)
		, m_isValid(true)
		, m_currentFrameNum(std::numeric_limits<uint64_t>::max())
	{
		for (uint8_t pbType = 0; pbType < re::ParameterBlock::PBType::PBType_Count; pbType++)
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);
				m_allocations[pbType].m_committed.reserve(k_systemMemoryReservationSize);
			}
		}

		platform::ParameterBlockAllocator::CreatePlatformParams(*this);
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
				SEAssert("Failed to clear the map", allocation.m_handleToPtr.empty());

				allocation.m_committed.clear();
			}

			// Clear the handle -> commit map
			m_handleToTypeAndByteIndex.clear();
		}

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
		SEAssert("Permanent parameter blocks can only be registered at startup, before the 1st render frame", 
			pb->GetType() == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded);

		const ParameterBlock::PBType pbType = pb->GetType();
		SEAssert("Invalid PBType", pbType != re::ParameterBlock::PBType::PBType_Count);

		{
			std::lock_guard<std::recursive_mutex> lock(m_allocations[pbType].m_mutex);

			SEAssert("Parameter block is already registered",
				!m_allocations[pbType].m_handleToPtr.contains(pb->GetUniqueID()));

			m_allocations[pbType].m_handleToPtr[pb->GetUniqueID()] = pb;
		}

		// Pre-allocate our PB so it's ready to commit to:
		Allocate(pb->GetUniqueID(), numBytes, pbType);
	}


	void ParameterBlockAllocator::Allocate(Handle uniqueID, uint32_t numBytes, ParameterBlock::PBType pbType)
	{
		SEAssert("Permanent parameter blocks can only be allocated at startup, before the 1st render frame",
			pbType == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded);

		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			SEAssert("A parameter block with this handle has already been added",
				m_handleToTypeAndByteIndex.find(uniqueID) == m_handleToTypeAndByteIndex.end());
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

			SEAssert("Parameter block with this ID has not been allocated",
				result != m_handleToTypeAndByteIndex.end());

			SEAssert("Immutable parameter blocks can only be committed at startup",
				!m_allocationPeriodEnded || result->second.m_type != ParameterBlock::PBType::Immutable);

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
	}


	void ParameterBlockAllocator::GetDataAndSize(Handle uniqueID, void const*& out_data, uint32_t& out_numBytes) const
	{
		ParameterBlock::PBType pbType;
		uint32_t startIdx = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);
			SEAssert("Parameter block with this ID has not been allocated",result != m_handleToTypeAndByteIndex.end());

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

		SEAssert("Parameter block with this ID has not been allocated",
			result != m_handleToTypeAndByteIndex.end());

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
			SEAssert("Cannot deallocate a parameter block that does not exist", pb != m_handleToTypeAndByteIndex.end());

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
	}


	// Buffer dirty PB data
	void ParameterBlockAllocator::BufferParamBlocks()
	{
		SEAssert("Cannot buffer param blocks until they're all allocated", m_allocationPeriodEnded);

		SEBeginCPUEvent("re::ParameterBlockAllocator::BufferParamBlocks");
		{
			std::lock_guard<std::mutex> dirtyLock(m_dirtyParameterBlocksMutex);

			while (!m_dirtyParameterBlocks.empty())
			{
				const Handle currentHandle = m_dirtyParameterBlocks.front();

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

				platform::ParameterBlock::Update(*currentPB);

				m_dirtyParameterBlocks.pop();
			}
		}
		SEEndCPUEvent();
	}


	void ParameterBlockAllocator::BeginFrame(uint64_t renderFrameNum)
	{
		m_currentFrameNum = renderFrameNum;
		m_platformParams->BeginFrame();
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
				SEAssert("Trying to deallocate a single frame parameter block, but there is still a live shared_ptr. Is "
					"something holding onto a single frame parameter block beyond the frame lifetime?",
					m_allocations[re::ParameterBlock::PBType::SingleFrame].m_handleToPtr.begin()->second.use_count() == 1);

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
		SEAssert("Trying to clear before the first swap buffer call", 
			m_currentFrameNum != std::numeric_limits<uint64_t>::max());

		SEBeginCPUEvent(
			std::format("ParameterBlockAllocator::ClearDeferredDeletions ({})", m_deferredDeleteQueue.size()).c_str());

		{
			std::lock_guard<std::mutex> lock(m_deferredDeleteQueueMutex);

			while (!m_deferredDeleteQueue.empty() && m_deferredDeleteQueue.front().first + k_deferredDeleteNumFrames < frameNum)
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