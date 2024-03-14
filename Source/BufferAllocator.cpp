// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "CastUtils.h"
#include "BufferAllocator.h"
#include "BufferAllocator_Platform.h"
#include "Buffer_Platform.h"
#include "ProfilingMarkers.h"
#include "RenderManager.h"
#include "RenderManager_Platform.h"


namespace
{
	constexpr uint64_t k_invalidFrameNum = std::numeric_limits<uint64_t>::max();
}

namespace re
{
	// Buffer Platform Params:
	//---------------------------------

	BufferAllocator::PlatformParams::PlatformParams()
		: m_numBuffers(platform::RenderManager::GetNumFramesInFlight())
		, m_writeIdx(0)
	{
		// We maintain N stack base indexes for each DataType; Initialize them to 0
		for (uint8_t dataType = 0; dataType < re::Buffer::DataType::DataType_Count; dataType++)
		{
			m_bufferBaseIndexes[dataType].store(0);
		}
	}


	void BufferAllocator::PlatformParams::BeginFrame()
	{
		// Increment the write index
		m_writeIdx = (m_writeIdx + 1) % m_numBuffers;

		// Reset the stack base index back to 0 for each type of shared buffer:
		for (uint8_t dataType = 0; dataType < re::Buffer::DataType::DataType_Count; dataType++)
		{
			m_bufferBaseIndexes[dataType].store(0);
		}
	}


	uint32_t BufferAllocator::PlatformParams::AdvanceBaseIdx(
		re::Buffer::DataType dataType, uint32_t alignedSize)
	{
		// Atomically advance the stack base index for the next call, and return the base index for the current one
		const uint32_t allocationBaseIdx = m_bufferBaseIndexes[dataType].fetch_add(alignedSize);

		SEAssert(allocationBaseIdx + alignedSize <= k_fixedAllocationByteSize,
			"Allocation is out of bounds. Consider increasing k_fixedAllocationByteSize");

		return allocationBaseIdx;
	}


	uint8_t BufferAllocator::PlatformParams::GetWriteIndex() const
	{
		return m_writeIdx;
	}


	// Buffer Allocator:
	//---------------------------

	BufferAllocator::PlatformParams* BufferAllocator::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	void BufferAllocator::SetPlatformParams(std::unique_ptr<BufferAllocator::PlatformParams> params)
	{
		m_platformParams = std::move(params);
	}


	BufferAllocator::BufferAllocator()
		: m_numFramesInFlight(3) // Safe default: We'll fetch the correct value during Create()
		, m_currentFrameNum(k_invalidFrameNum)
		, m_isValid(false)
		, m_maxSingleFrameAllocations(0) // Debug: Track the high-water mark for the max single-frame buffer allocations
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

		platform::BufferAllocator::CreatePlatformParams(*this);
	}


	void BufferAllocator::Create(uint64_t currentFrame)
	{
		m_currentFrameNum = currentFrame;
		platform::BufferAllocator::Create(*this);

		m_numFramesInFlight = re::RenderManager::GetNumFramesInFlight();

		m_isValid = true;
	}


	BufferAllocator::~BufferAllocator()
	{
		SEAssert(!IsValid(),
			"Buffer allocator destructor called before Destroy(). The buffer allocator must "
			"be manually destroyed (i.e. in the api-specific Context::Destroy())");
	}
	

	void BufferAllocator::Destroy()
	{
		{
			std::scoped_lock lock(
				m_handleToTypeAndByteIndexMutex,
				m_mutableAllocations.m_mutex,
				m_immutableAllocations.m_mutex,
				m_singleFrameAllocations.m_mutex);

			// Sum the number of bytes used by our permanent buffers:
			size_t numMutableBufferBytes = 0;
			for (size_t i = 0; i < m_mutableAllocations.m_committed.size(); i++)
			{
				numMutableBufferBytes += m_mutableAllocations.m_committed[i].size();
			}
			size_t numImmutableBufferBytes = 0;
			for (size_t i = 0; i < m_immutableAllocations.m_committed.size(); i++)
			{
				numImmutableBufferBytes += m_immutableAllocations.m_committed[i].size();
			}

			LOG(std::format("BufferAllocator shutting down. Session usage statistics:\n"
				"\t{} Immutable permanent allocations: {} B\n"
				"\t{} Mutable permanent allocations: {} B\n"
				"\t{} max single-frame allocations, max {} B buffer usage seen",
				m_immutableAllocations.m_handleToPtr.size(),
				numImmutableBufferBytes,
				m_mutableAllocations.m_handleToPtr.size(),
				numMutableBufferBytes,
				m_maxSingleFrameAllocations,
				m_maxSingleFrameAllocationByteSize).c_str());

			// Must clear the buffers shared_ptrs before clearing the committed memory
			// Destroy() removes the buffer from our unordered_map & invalidates iterators; Just loop until it's empty
			auto ClearBufferPtrs = [](std::unordered_map<Handle, std::shared_ptr<re::Buffer>>& handleToPtr)
				{
					while (!handleToPtr.empty())
					{
						handleToPtr.begin()->second->Destroy();
					}
					SEAssert(handleToPtr.empty(), "Failed to clear the map");
				};
			ClearBufferPtrs(m_mutableAllocations.m_handleToPtr);
			ClearBufferPtrs(m_immutableAllocations.m_handleToPtr);
			ClearBufferPtrs(m_singleFrameAllocations.m_handleToPtr);

			SEAssert(m_mutableAllocations.m_committed.empty(), "Mutable committed data should be cleared by now");
			SEAssert(m_immutableAllocations.m_committed.empty(), "Immutable committed data should be cleared by now");
			SEAssert(m_singleFrameAllocations.m_committed.empty(), "Single frame committed data should be cleared by now");

			SEAssert(m_handleToTypeAndByteIndex.empty(), "Handle to type and byte map should be cleared by now");
		}

		m_dirtyBuffers.clear();

		// The platform::RenderManager has already flushed all outstanding work; Force our deferred deletions to be
		// immediately cleared
		constexpr uint64_t k_maxFrameNum = std::numeric_limits<uint64_t>::max();
		ClearDeferredDeletions(k_maxFrameNum);

		platform::BufferAllocator::Destroy(*this);

		m_isValid = false;
	}


	bool BufferAllocator::IsValid() const
	{
		return m_isValid;
	}


	void BufferAllocator::RegisterAndAllocateBuffer(
		std::shared_ptr<re::Buffer> buffer, uint32_t numBytes)
	{
		const Buffer::Type bufferType = buffer->GetType();
		SEAssert(bufferType != re::Buffer::Type::Type_Count, "Invalid Type");

		const Handle uniqueID = buffer->GetUniqueID();

		auto RecordHandleToPointer = [&](
			std::recursive_mutex& mutex,
			std::unordered_map<Handle, std::shared_ptr<re::Buffer>>& handleToPtr)
			{
				std::lock_guard<std::recursive_mutex> lock(mutex);
				SEAssert(!handleToPtr.contains(uniqueID), "Buffer is already registered");
				handleToPtr[uniqueID] = buffer;
			};

		switch (bufferType)
		{
		case Buffer::Type::Mutable:
		{
			RecordHandleToPointer(m_mutableAllocations.m_mutex, m_mutableAllocations.m_handleToPtr);
		}
		break;
		case Buffer::Type::Immutable:
		{
			RecordHandleToPointer(m_immutableAllocations.m_mutex, m_immutableAllocations.m_handleToPtr);
		}
		break;
		case Buffer::Type::SingleFrame:
		{
			RecordHandleToPointer(m_singleFrameAllocations.m_mutex, m_singleFrameAllocations.m_handleToPtr);
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		// Pre-allocate our buffer so it's ready to commit to:
		Allocate(uniqueID, numBytes, bufferType);
	}


	void BufferAllocator::Allocate(Handle uniqueID, uint32_t numBytes, Buffer::Type bufferType)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			SEAssert(m_handleToTypeAndByteIndex.find(uniqueID) == m_handleToTypeAndByteIndex.end(),
				"A buffer with this handle has already been added");
		}

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		uint32_t dataIndex = -1; // Start with something obviously incorrect

		switch (bufferType)
		{
		case Buffer::Type::Mutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				dataIndex = util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed.size());
				m_mutableAllocations.m_committed.emplace_back(numBytes, 0);
			}
		}
		break;
		case Buffer::Type::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			dataIndex = util::CheckedCast<uint32_t>(m_immutableAllocations.m_committed.size());
			m_immutableAllocations.m_committed.emplace_back(numBytes, 0);
		}
		break;
		case Buffer::Type::SingleFrame:
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
		default: SEAssertF("Invalid Type");
		}

		// Update our ID -> data tracking table:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);
			m_handleToTypeAndByteIndex.insert({ uniqueID, {bufferType, dataIndex, numBytes} });
		}
	}


	void BufferAllocator::Commit(Handle uniqueID, void const* data)
	{
		uint32_t startIdx;
		uint32_t numBytes;
		Buffer::Type bufferType;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);

			SEAssert(result != m_handleToTypeAndByteIndex.end(),
				"Buffer with this ID has not been allocated");

			startIdx = result->second.m_startIndex;
			numBytes = result->second.m_numBytes;
			bufferType = result->second.m_type;
		}


		// Copy the data to our pre-allocated region.
		switch (bufferType)
		{
		case Buffer::Type::Mutable:
		{
			Commit(uniqueID, data, numBytes, 0);
		}
		break;
		case Buffer::Type::Immutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				SEAssert(m_immutableAllocations.m_committed[startIdx].size() == numBytes, "Size mismatch");
				void* dest = m_immutableAllocations.m_committed[startIdx].data();
				memcpy(dest, data, numBytes);
			}
		}
		break;
		case Buffer::Type::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				void* dest = &m_singleFrameAllocations.m_committed[startIdx];
				memcpy(dest, data, numBytes);
			}
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		// Add the committed buffer to our dirty list, so we can buffer the data when required
		if (bufferType != Buffer::Type::Mutable) // Mutables have their own commit path: They add themselves there
		{
			std::lock_guard<std::mutex> lock(m_dirtyBuffersMutex);
			m_dirtyBuffers.emplace(uniqueID);
		}
	}


	void BufferAllocator::Commit(
		Handle uniqueID, void const* data, uint32_t numBytes, uint32_t dstBaseByteOffset)
	{
		SEAssert(numBytes > 0, "0 bytes is only valid for signalling the Buffer::Update to update all bytes");

		uint32_t startIdx;
		uint32_t totalBytes;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);

			SEAssert(result != m_handleToTypeAndByteIndex.end(),
				"Buffer with this ID has not been allocated");

			SEAssert(result->second.m_type == re::Buffer::Type::Mutable,
				"Can only partially commit to mutable buffers");
			
			startIdx = result->second.m_startIndex;
			totalBytes = result->second.m_numBytes;

			SEAssert(numBytes <= totalBytes, "Trying to commit more data than is allocated");
		}

		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			SEAssert(totalBytes == util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed[startIdx].size()),
				"CommitMetadata and physical allocation out of sync");

			SEAssert(dstBaseByteOffset + numBytes <= totalBytes,
				"Number of bytes is too large for the given offset");

			// Copy the data into our CPU-side allocation:
			void* dest = &m_mutableAllocations.m_committed[startIdx][dstBaseByteOffset]; // Byte address of base offset
			memcpy(dest, data, numBytes);

			// Find or insert a commit record for the given base offset:
			MutableAllocation::CommitRecord& commitRecord = m_mutableAllocations.m_partialCommits[uniqueID];

			if (numBytes == totalBytes)
			{
				// If we're committing all bytes, remove any other commits as we're guaranteed to write the data anyway
				commitRecord.clear();
				commitRecord.push_back(MutableAllocation::PartialCommit{
						.m_baseOffset = 0,
						.m_numBytes = numBytes,
						.m_numRemainingUpdates = m_numFramesInFlight });
			}
			else
			{
				auto GetSortedInsertionPointItr = [&](MutableAllocation::PartialCommit const& newCommit) -> MutableAllocation::CommitRecord::iterator
					{
						return std::upper_bound(
							commitRecord.begin(),
							commitRecord.end(),
							newCommit,
							[](MutableAllocation::PartialCommit const& a, MutableAllocation::PartialCommit const& b)
							{
								if (a.m_baseOffset == b.m_baseOffset)
								{
									return a.m_numBytes < b.m_numBytes;
								}
								return a.m_baseOffset < b.m_baseOffset;
							});
					};

				MutableAllocation::PartialCommit newCommit = MutableAllocation::PartialCommit{
					.m_baseOffset = dstBaseByteOffset,
					.m_numBytes = numBytes,
					.m_numRemainingUpdates = m_numFramesInFlight };

				auto prev = commitRecord.insert(GetSortedInsertionPointItr(newCommit), newCommit);
				if (prev != commitRecord.begin())
				{
					--prev; // Start our search from the element before, incase it overlaps
				}
				auto current = std::next(prev);

				// Patch the existing entries:
				while (current != commitRecord.end() && prev->m_baseOffset + prev->m_numBytes >= current->m_baseOffset)
				{
					SEAssert(prev->m_baseOffset <= current->m_baseOffset,
						"Previous and current are out of order");

					uint32_t prevFirstOOBByte = prev->m_baseOffset + prev->m_numBytes;

					// Previous commit entirely overlaps the current one. Split the previous entry
					if (prevFirstOOBByte > (current->m_baseOffset + current->m_numBytes))
					{
						if (prev->m_numRemainingUpdates != current->m_numRemainingUpdates)
						{
							const MutableAllocation::PartialCommit lowerSplit = MutableAllocation::PartialCommit{
									.m_baseOffset = prev->m_baseOffset,
									.m_numBytes = (current->m_baseOffset - prev->m_baseOffset),
									.m_numRemainingUpdates = prev->m_numRemainingUpdates};

							const MutableAllocation::PartialCommit upperSplit = MutableAllocation::PartialCommit{
									.m_baseOffset = current->m_baseOffset,
									.m_numBytes = prevFirstOOBByte - current->m_baseOffset,
									.m_numRemainingUpdates = prev->m_numRemainingUpdates};

							commitRecord.erase(prev);

							current = commitRecord.insert(
								GetSortedInsertionPointItr(lowerSplit),
								lowerSplit);

							commitRecord.insert(
								GetSortedInsertionPointItr(upperSplit),
								upperSplit);

							if (current == commitRecord.begin())
							{
								prev = current;
								++current;
							}
							else
							{
								prev = std::prev(current);
							}							
						}
						else
						{
							// Total overlap from 2 records on the same frame. Just delete the smaller one
							commitRecord.erase(current);
							current = std::next(prev);
						}

						continue;
					}

					// Overlapping commits made during the same frame. Merge them:
					if (prev->m_numRemainingUpdates == current->m_numRemainingUpdates)
					{
						current->m_numBytes += current->m_baseOffset - prev->m_baseOffset;
						current->m_baseOffset = prev->m_baseOffset;

						commitRecord.erase(prev);
						prev = commitRecord.end();
					}
					else // Overlapping commits from different frames. Prune the oldest:
					{
						if (prevFirstOOBByte > current->m_baseOffset)
						{
							// prev is oldest:
							if (prev->m_numRemainingUpdates < current->m_numRemainingUpdates)
							{
								prev->m_numBytes -= (prevFirstOOBByte - current->m_baseOffset);
							}
							else // current is oldest
							{
								current->m_numBytes -= (prevFirstOOBByte - current->m_baseOffset);
								current->m_baseOffset = prevFirstOOBByte;
							}
						}
					}

					// Prepare for the next iteration:
					if (prev != commitRecord.end() && prev->m_numBytes == 0)
					{
						if (prev == commitRecord.begin())
						{
							commitRecord.erase(prev);
							prev = commitRecord.begin();
							current = std::next(prev);
						}
						else
						{
							auto temp = std::prev(prev);
							commitRecord.erase(prev);
							prev = temp;
						}
					}
					else if (current->m_numBytes == 0)
					{
						auto temp = std::next(current);
						commitRecord.erase(current);
						current = temp;
					}
					else
					{
						prev = current;
						++current;
					}
				}
			}
		}

		// Add the mutable buffer to our dirty list, so we can buffer the data when required
		{
			std::lock_guard<std::mutex> lock(m_dirtyBuffersMutex);
			m_dirtyBuffers.emplace(uniqueID); // Does nothing if the uniqueID was already recorded
		}
	}


	void BufferAllocator::GetDataAndSize(Handle uniqueID, void const*& out_data, uint32_t& out_numBytes) const
	{
		Buffer::Type bufferType;
		uint32_t startIdx = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);
			SEAssert(result != m_handleToTypeAndByteIndex.end(), "Buffer with this ID has not been allocated");

			bufferType = result->second.m_type;
			startIdx = result->second.m_startIndex;

			out_numBytes = result->second.m_numBytes;
		}

		// Note: This is not thread safe, as the pointer will become stale if m_committed is resized. This should be
		// fine though, as the BufferAllocator is simply a temporary staging ground for data about to be copied
		// to GPU heaps. Copies in/resizing should all be done before this function is ever called
		switch (bufferType)
		{
		case Buffer::Type::Mutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				out_data = m_mutableAllocations.m_committed[startIdx].data();
			}
		}
		break;
		case Buffer::Type::Immutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				out_data = m_immutableAllocations.m_committed[startIdx].data();
			}
		}
		break;
		case Buffer::Type::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				out_data = &m_singleFrameAllocations.m_committed[startIdx];
			}
		}
		break;
		default: SEAssertF("Invalid Type");
		}
	}


	uint32_t BufferAllocator::GetSize(Handle uniqueID) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

		auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);

		SEAssert(result != m_handleToTypeAndByteIndex.end(), "Buffer with this ID has not been allocated");

		return result->second.m_numBytes;
	}


	void BufferAllocator::Deallocate(Handle uniqueID)
	{
		Buffer::Type bufferType = re::Buffer::Type::Type_Count;
		uint32_t startIdx = -1;
		uint32_t numBytes = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& buffer = m_handleToTypeAndByteIndex.find(uniqueID);
			SEAssert(buffer != m_handleToTypeAndByteIndex.end(), "Cannot deallocate a buffer that does not exist");

			bufferType = buffer->second.m_type;
			startIdx = buffer->second.m_startIndex;
			numBytes = buffer->second.m_numBytes;
		}

		// Add our buffer to the deferred deletion queue, then erase the pointer from our allocation list
		auto ProcessErasure = [&](
			std::unordered_map<Handle, std::shared_ptr<re::Buffer>>& handleToPtr,
			std::recursive_mutex& mutex)
			{
				AddToDeferredDeletions(m_currentFrameNum, handleToPtr.at(uniqueID));

				// Erase the buffer from our allocations:
				{
					std::lock_guard<std::recursive_mutex> lock(mutex);
					handleToPtr.erase(uniqueID);
				}
			};
		switch (bufferType)
		{
		case Buffer::Type::Mutable:
		{
			ProcessErasure(m_mutableAllocations.m_handleToPtr, m_mutableAllocations.m_mutex);
			
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				m_mutableAllocations.m_partialCommits.erase(uniqueID);
			}
		}
		break;
		case Buffer::Type::Immutable:
		{
			ProcessErasure(m_immutableAllocations.m_handleToPtr, m_immutableAllocations.m_mutex);
		}
		break;
		case Buffer::Type::SingleFrame:
		{
			ProcessErasure(m_singleFrameAllocations.m_handleToPtr, m_singleFrameAllocations.m_mutex);
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		// Remove the handle from our map:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& buffer = m_handleToTypeAndByteIndex.find(uniqueID);
			m_handleToTypeAndByteIndex.erase(buffer);
		}

		// Finally, free the committed memory:
		auto FreePermanentCommit = [&](
			Buffer::Type bufferType,
			std::recursive_mutex& mutex,
			std::vector<std::vector<uint8_t>>& committed)
			{
				{
					std::scoped_lock lock(mutex, m_handleToTypeAndByteIndexMutex);

					const size_t idxToReplace = startIdx;
					const size_t idxToMove = committed.size() - 1;

					SEAssert(idxToReplace <= idxToMove &&
						idxToReplace < committed.size(),
						"Invalid index to move or replace");

					if (idxToReplace != idxToMove)
					{
						committed[idxToReplace] = std::move(committed[idxToMove]);

						// Update the records for the entry that we moved. This is a slow linear search through an
						// unordered map, but permanent buffers should be deallocated very infrequently
						bool didUpdate = false;
						for (auto& entry : m_handleToTypeAndByteIndex)
						{
							if (entry.second.m_type == bufferType && entry.second.m_startIndex == idxToMove)
							{
								entry.second.m_startIndex = util::CheckedCast<uint32_t>(idxToReplace);
								didUpdate = true;
								break;
							}
						}
						SEAssert(didUpdate, "Failed to find entry to update");
					}
					committed.pop_back();
				}
			};
		switch (bufferType)
		{
		case Buffer::Type::Mutable:
		{
			FreePermanentCommit(
				Buffer::Type::Mutable, m_mutableAllocations.m_mutex, m_mutableAllocations.m_committed);
		}
		break;
		case Buffer::Type::Immutable:
		{
			FreePermanentCommit(Buffer::Type::Immutable,
				m_immutableAllocations.m_mutex, m_immutableAllocations.m_committed);
		}
		break;
		case Buffer::Type::SingleFrame:
		{
			// Single frame buffer memory is already cleared at the end of every frame
		}
		break;
		default: SEAssertF("Invalid Type");
		}
	}


	// Buffer dirty data
	void BufferAllocator::BufferData()
	{
		SEBeginCPUEvent("re::BufferAllocator::BufferData");
		{
			std::lock_guard<std::mutex> dirtyLock(m_dirtyBuffersMutex);

			// We keep mutable buffers committed within m_numFramesInFlight in the dirty list to ensure they're
			// kept up to date
			std::unordered_set<Handle> dirtyMutableBuffers;

			const uint8_t curFrameHeapOffsetFactor = m_currentFrameNum % m_numFramesInFlight; // Only used for mutable buffers

			for (Handle currentHandle : m_dirtyBuffers)
			{
				Buffer::Type bufferType = Buffer::Type::Type_Count;
				{
					std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);
					bufferType = m_handleToTypeAndByteIndex.find(currentHandle)->second.m_type;
				}

				re::Buffer const* currentBuffer = nullptr;

				auto GetCurrentBufferPtr = [&](
					std::recursive_mutex& mutex, 
					std::unordered_map<Handle, std::shared_ptr<re::Buffer>> const& handleToPtr)
					{
						std::lock_guard<std::recursive_mutex> lock(mutex);
						SEAssert(handleToPtr.contains(currentHandle), "Buffer is not registered");
						currentBuffer = handleToPtr.at(currentHandle).get();
					};
				switch (bufferType)
				{
				case Buffer::Type::Mutable:
				{
					GetCurrentBufferPtr(m_mutableAllocations.m_mutex, m_mutableAllocations.m_handleToPtr);
				}
				break;
				case Buffer::Type::Immutable:
				{
					GetCurrentBufferPtr(m_immutableAllocations.m_mutex, m_immutableAllocations.m_handleToPtr);
				}
				break;
				case Buffer::Type::SingleFrame:
				{
					GetCurrentBufferPtr(m_singleFrameAllocations.m_mutex, m_singleFrameAllocations.m_handleToPtr);
				}
				break;
				default: SEAssertF("Invalid Type");
				}

				SEAssert(currentBuffer->GetPlatformParams()->m_isCommitted, 
					"Trying to buffer a buffer that has not had an initial commit made");

				// Perform each of the partial commits recorded for Mutable buffers:
				if (bufferType == Buffer::Type::Mutable)
				{
					{
						// NOTE: This is a potential deadlock risk as we already hold the m_dirtyBuffersMutex.
						// It's safe for now, but leaving this comment here in case things change...
						std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

						SEAssert(m_mutableAllocations.m_partialCommits.contains(currentHandle),
							"Cannot find mutable buffer, was it ever committed?");

						MutableAllocation::CommitRecord& commitRecords =
							m_mutableAllocations.m_partialCommits.at(currentHandle);
						
						auto partialCommit = commitRecords.begin();
						while (partialCommit != commitRecords.end())
						{
							platform::Buffer::Update(
								*currentBuffer,
								curFrameHeapOffsetFactor,
								partialCommit->m_baseOffset,
								partialCommit->m_numBytes);

							// Decrement the remaining updates counter: If 0, the commit has been fully propogated to 
							// all  buffers and we can remove it
							partialCommit->m_numRemainingUpdates--;
							if (partialCommit->m_numRemainingUpdates == 0)
							{
								auto itrToDelete = partialCommit;
								++partialCommit;
								commitRecords.erase(itrToDelete);
							}
							else
							{
								dirtyMutableBuffers.emplace(currentHandle); // Does nothing if the buffer was already recorded
								++partialCommit;
							}
						}
					}
				}
				else
				{
					platform::Buffer::Update(*currentBuffer, curFrameHeapOffsetFactor, 0, 0);
				}
			}

			// Swap in our dirty list for the next frame
			m_dirtyBuffers = std::move(dirtyMutableBuffers);
		}

		SEEndCPUEvent();
	}


	void BufferAllocator::BeginFrame(uint64_t renderFrameNum)
	{
		// Avoid stomping existing data when the BufferAllocator has already been accessed (e.g. during
		// RenderManager::Initialize, before BufferAllocator::BeginFrame has been called)
		if (renderFrameNum != m_currentFrameNum)
		{
			m_currentFrameNum = renderFrameNum;
			m_platformParams->BeginFrame();
		}
	}


	void BufferAllocator::EndFrame()
	{
		SEBeginCPUEvent("re::BufferAllocator::EndFrame");

		// Clear single-frame allocations:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			// Debug: Track the high-water mark for the max single-frame buffer allocations
			m_maxSingleFrameAllocations = std::max(
				m_maxSingleFrameAllocations,
				util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_handleToPtr.size()));
			m_maxSingleFrameAllocationByteSize = std::max(
				m_maxSingleFrameAllocationByteSize,
				util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size()));

			// Calling Destroy() on our Buffer recursively calls BufferAllocator::Deallocate, which
			// erases an entry from m_singleFrameAllocations.m_handleToPtr. Thus, we can't use an iterator as it'll be
			// invalidated. Instead, we just loop until it's empty
			while (!m_singleFrameAllocations.m_handleToPtr.empty())
			{
				SEAssert(m_singleFrameAllocations.m_handleToPtr.begin()->second.use_count() == 1,
					"Trying to deallocate a single frame buffer, but there is still a live shared_ptr. Is "
					"something holding onto a single frame buffer beyond the frame lifetime?");

				m_singleFrameAllocations.m_handleToPtr.begin()->second->Destroy();
			}

			m_singleFrameAllocations.m_handleToPtr.clear();
			m_singleFrameAllocations.m_committed.clear();
		}

		ClearDeferredDeletions(m_currentFrameNum);

		SEEndCPUEvent();
	}


	void BufferAllocator::ClearDeferredDeletions(uint64_t frameNum)
	{
		SEAssert(m_currentFrameNum != std::numeric_limits<uint64_t>::max(),
			"Trying to clear before the first swap buffer call");

		SEBeginCPUEvent(
			std::format("BufferAllocator::ClearDeferredDeletions ({})", m_deferredDeleteQueue.size()).c_str());

		{
			std::lock_guard<std::mutex> lock(m_deferredDeleteQueueMutex);

			while (!m_deferredDeleteQueue.empty() &&
				m_deferredDeleteQueue.front().first + m_numFramesInFlight < frameNum)
			{
				platform::Buffer::Destroy(*m_deferredDeleteQueue.front().second);
				m_deferredDeleteQueue.pop();
			}
		}

		SEEndCPUEvent();
	}


	void BufferAllocator::AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::Buffer> buffer)
	{
		std::lock_guard<std::mutex> lock(m_deferredDeleteQueueMutex);

		m_deferredDeleteQueue.emplace(std::pair<uint64_t, std::shared_ptr<re::Buffer>>{frameNum, buffer});
	}
}