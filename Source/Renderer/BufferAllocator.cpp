// © 2022 Adam Badke. All rights reserved.
#include "BufferAllocator.h"
#include "BufferAllocator_DX12.h"
#include "BufferAllocator_OpenGL.h"
#include "Buffer_Platform.h"
#include "RenderManager.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/ProfilingMarkers.h"

#include "Core/Util/CastUtils.h"


namespace
{
	constexpr uint64_t k_invalidFrameNum = std::numeric_limits<uint64_t>::max();
	constexpr uint32_t k_invalidCommitValue = std::numeric_limits<uint32_t>::max();
}

namespace re
{
	std::unique_ptr<re::BufferAllocator> BufferAllocator::Create()
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			return std::make_unique<opengl::BufferAllocator>();
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			return std::make_unique<dx12::BufferAllocator>();
		}
		break;
		default:
			SEAssertF("Invalid rendering API argument received");
		}
		return nullptr;
	}


	BufferAllocator::BufferAllocator()
		: m_numFramesInFlight(0) // We'll fetch the correct value during Create()
		, m_writeIdx(0)
		, m_currentFrameNum(k_invalidFrameNum)
		, m_isValid(false)
	{
		// We maintain N stack base indexes for each Type; Initialize them to 0
		for (uint8_t dataType = 0; dataType < re::Buffer::Type::Type_Count; dataType++)
		{
			m_bufferBaseIndexes[dataType].store(0);
		}

		// Mutable allocations:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
			m_mutableAllocations.m_committed.reserve(k_permanentReservationCount);

			m_mutableAllocations.m_totalAllocations = 0;
			m_mutableAllocations.m_totalAllocationsByteSize = 0;
			m_mutableAllocations.m_currentAllocationsByteSize = 0;
			m_mutableAllocations.m_maxAllocations = 0;
			m_mutableAllocations.m_maxAllocationsByteSize = 0;
		}

		// Temporary allocations:
		auto InitializeTemporaryAllocation = [](TemporaryAllocation& tempAllocation)
			{
				std::lock_guard<std::recursive_mutex> lock(tempAllocation.m_mutex);
				tempAllocation.m_committed.reserve(k_temporaryReservationBytes);

				tempAllocation.m_totalAllocations = 0;
				tempAllocation.m_currentAllocationsByteSize = 0;
				tempAllocation.m_maxAllocations = 0;
				tempAllocation.m_maxAllocationsByteSize = 0;
				tempAllocation.m_totalAllocationsByteSize = 0;
			};
		InitializeTemporaryAllocation(m_immutableAllocations);
		InitializeTemporaryAllocation(m_singleFrameAllocations);
	}


	void BufferAllocator::Initialize(uint64_t currentFrame)
	{
		m_currentFrameNum = currentFrame;
		m_numFramesInFlight = re::RenderManager::GetNumFramesInFlight();
		m_isValid = true;

		m_writeIdx = 0;
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

			LOG(std::format("BufferAllocator shutting down... Session usage statistics:\n"
				"\t\t- {} Mutable permanent allocations total, {} B lifetime total, {} / {} B max simultaneous\n"
				"\t\t- {} Immutable permanent allocations total, {} B lifetime total, {} / {} B max simultaneous\n"				
				"\t\t- {} Single frame allocations total, {} B lifetime total, {} / {} B max simultaneous",
				m_mutableAllocations.m_totalAllocations,
				m_mutableAllocations.m_totalAllocationsByteSize,
				m_mutableAllocations.m_maxAllocations,
				m_mutableAllocations.m_maxAllocationsByteSize,
				m_immutableAllocations.m_totalAllocations,
				m_immutableAllocations.m_totalAllocationsByteSize,
				m_immutableAllocations.m_maxAllocations,
				m_immutableAllocations.m_maxAllocationsByteSize,
				m_singleFrameAllocations.m_totalAllocations,
				m_singleFrameAllocations.m_totalAllocationsByteSize,
				m_singleFrameAllocations.m_maxAllocations,
				m_singleFrameAllocations.m_maxAllocationsByteSize).c_str());

			if (m_mutableAllocations.m_maxAllocations >= k_permanentReservationCount)
			{
				LOG_WARNING("Mutable allocations required more than the default reservation amount. Consider "
					"increasing k_permanentReservationCount");
			}
			if (m_immutableAllocations.m_maxAllocationsByteSize >= k_temporaryReservationBytes)
			{
				LOG_WARNING("Immutable allocations required more than the default reservation amount. Consider "
					"increasing k_temporaryReservationBytes");
			}
			if (m_singleFrameAllocations.m_maxAllocationsByteSize >= k_temporaryReservationBytes)
			{
				LOG_WARNING("Single frame allocations required more than the default reservation amount. Consider "
					"increasing k_temporaryReservationBytes");
			}

			// Must clear the buffers shared_ptrs before clearing the committed memory
			// Destroy() removes the buffer from our unordered_map & invalidates iterators; Just loop until it's empty
			auto ClearBufferPtrs = [](IAllocation& allocation)
				{
					while (!allocation.m_handleToPtr.empty())
					{
						allocation.m_handleToPtr.begin()->second->Destroy();
					}
					SEAssert(allocation.m_handleToPtr.empty(), "Failed to clear the map");
				};
			ClearBufferPtrs(m_mutableAllocations);
			ClearBufferPtrs(m_immutableAllocations);
			ClearBufferPtrs(m_singleFrameAllocations);

			SEAssert(m_mutableAllocations.m_currentAllocationsByteSize == 0 &&
				m_immutableAllocations.m_currentAllocationsByteSize == 0 &&
				m_singleFrameAllocations.m_currentAllocationsByteSize == 0,
				"Deallocations and tracking data are out of sync");

			SEAssert(m_handleToTypeAndByteIndex.empty(), "Handle to type and byte map should be cleared by now");
		}

		m_dirtyBuffers.clear();

		// The platform::RenderManager has already flushed all outstanding work; Force our deferred deletions to be
		// immediately cleared
		constexpr uint64_t k_maxFrameNum = std::numeric_limits<uint64_t>::max();
		ClearDeferredDeletions(k_maxFrameNum);

		m_isValid = false;
	}


	void BufferAllocator::RegisterAndAllocateBuffer(std::shared_ptr<re::Buffer> buffer, uint32_t numBytes)
	{
		const Buffer::AllocationType bufferAlloc = buffer->GetAllocationType();
		SEAssert(bufferAlloc != re::Buffer::AllocationType::AllocationType_Invalid, "Invalid AllocationType");

		const Handle uniqueID = buffer->GetUniqueID();

		auto RecordHandleToPointer = [&](IAllocation& allocation)
			{
				std::lock_guard<std::recursive_mutex> lock(allocation.m_mutex);
				SEAssert(!allocation.m_handleToPtr.contains(uniqueID), "Buffer is already registered");
				allocation.m_handleToPtr[uniqueID] = buffer;
			};

		switch (bufferAlloc)
		{
		case Buffer::AllocationType::Mutable:
		{
			RecordHandleToPointer(m_mutableAllocations);
		}
		break;
		case Buffer::AllocationType::Immutable:
		{
			RecordHandleToPointer(m_immutableAllocations);
		}
		break;
		case Buffer::AllocationType::SingleFrame:
		{
			RecordHandleToPointer(m_singleFrameAllocations);
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		// Pre-allocate our buffer so it's ready to commit to:
		Allocate(uniqueID, numBytes, bufferAlloc);
	}


	void BufferAllocator::Allocate(Handle uniqueID, uint32_t numBytes, Buffer::AllocationType bufferAlloc)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			SEAssert(m_handleToTypeAndByteIndex.find(uniqueID) == m_handleToTypeAndByteIndex.end(),
				"A buffer with this handle has already been added");
		}

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		uint32_t dataIndex = -1; // Start with something obviously incorrect

		auto UpdateAllocationTracking = [&numBytes](IAllocation& allocation)
			{
				// Note: IAllocation::m_mutex is already locked

				allocation.m_totalAllocations++;
				allocation.m_totalAllocationsByteSize += numBytes;
				allocation.m_currentAllocationsByteSize += numBytes;
				allocation.m_maxAllocations = std::max(
					allocation.m_maxAllocations,
					util::CheckedCast<uint32_t>(allocation.m_handleToPtr.size()));
				allocation.m_maxAllocationsByteSize = std::max(
					allocation.m_maxAllocationsByteSize,
					allocation.m_currentAllocationsByteSize);
			};

		switch (bufferAlloc)
		{
		case Buffer::AllocationType::Mutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				dataIndex = util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed.size());
				m_mutableAllocations.m_committed.emplace_back(numBytes, 0);

				UpdateAllocationTracking(m_mutableAllocations);
			}
		}
		break;
		case Buffer::AllocationType::Immutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);

				dataIndex = util::CheckedCast<uint32_t>(m_immutableAllocations.m_committed.size());

				m_immutableAllocations.m_committed.resize(
					util::CheckedCast<uint32_t>(m_immutableAllocations.m_committed.size() + numBytes), 
					0);

				UpdateAllocationTracking(m_immutableAllocations);
			}
		}
		break;
		case Buffer::AllocationType::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

				dataIndex = util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size());

				m_singleFrameAllocations.m_committed.resize(
					util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size() + numBytes),
					0);

				UpdateAllocationTracking(m_singleFrameAllocations);
			}
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		// Update our ID -> data tracking table:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);
			m_handleToTypeAndByteIndex.insert({ uniqueID, CommitMetadata{bufferAlloc, dataIndex, numBytes} });
		}
	}


	void BufferAllocator::Commit(Handle uniqueID, void const* data)
	{
		uint32_t startIdx;
		uint32_t numBytes;
		Buffer::AllocationType bufferAlloc;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);

			SEAssert(result != m_handleToTypeAndByteIndex.end(),
				"Buffer with this ID has not been allocated");

			startIdx = result->second.m_startIndex;
			numBytes = result->second.m_numBytes;
			bufferAlloc = result->second.m_allocationType;
		}

		// Copy the data to our pre-allocated region.
		switch (bufferAlloc)
		{
		case Buffer::AllocationType::Mutable:
		{
			Commit(uniqueID, data, numBytes, 0);
		}
		break;
		case Buffer::AllocationType::Immutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				void* dest = &m_immutableAllocations.m_committed[startIdx];
				memcpy(dest, data, numBytes);
			}
		}
		break;
		case Buffer::AllocationType::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				void* dest = &m_singleFrameAllocations.m_committed[startIdx];
				memcpy(dest, data, numBytes);
			}
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		// Add the committed buffer to our dirty list, so we can buffer the data when required
		if (bufferAlloc != Buffer::AllocationType::Mutable) // Mutables have their own commit path: They add themselves there
		{
			std::lock_guard<std::mutex> lock(m_dirtyBuffersMutex);
			m_dirtyBuffers.emplace(uniqueID);
		}
	}


	void BufferAllocator::Commit(Handle uniqueID, void const* data, uint32_t numBytes, uint32_t dstBaseByteOffset)
	{
		SEAssert(numBytes > 0, "0 bytes is only valid for signalling the Buffer::Update to update all bytes");

		uint32_t startIdx;
		uint32_t totalBytes;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);

			SEAssert(result != m_handleToTypeAndByteIndex.end(), "Buffer with this ID has not been allocated");
			SEAssert(result->second.m_allocationType == re::Buffer::AllocationType::Mutable, "Can only partially commit to mutable buffers");
			
			startIdx = result->second.m_startIndex;
			totalBytes = result->second.m_numBytes;

			SEAssert(numBytes <= totalBytes, "Trying to commit more data than is allocated");
		}

		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			SEAssert(totalBytes == util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed[startIdx].size()),
				"CommitMetadata and physical allocation out of sync");

			SEAssert(dstBaseByteOffset + numBytes <= totalBytes, "Number of bytes is too large for the given offset");

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
				auto GetSortedInsertionPointItr = [&](
					MutableAllocation::PartialCommit const& newCommit) -> MutableAllocation::CommitRecord::iterator
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


	void BufferAllocator::GetData(Handle uniqueID, void const** out_data) const
	{
		Buffer::AllocationType bufferAlloc;
		uint32_t startIdx = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& result = m_handleToTypeAndByteIndex.find(uniqueID);
			SEAssert(result != m_handleToTypeAndByteIndex.end(), "Buffer with this ID has not been allocated");

			bufferAlloc = result->second.m_allocationType;
			startIdx = result->second.m_startIndex;
		}

		// Note: This is not thread safe, as the pointer will become stale if m_committed is resized. This should be
		// fine though, as the BufferAllocator is simply a temporary staging ground for data about to be copied
		// to GPU heaps. Copies in/resizing should all be done before this function is ever called
		switch (bufferAlloc)
		{
		case Buffer::AllocationType::Mutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				SEAssert(startIdx < m_mutableAllocations.m_committed.size(), "Invalid startIdx");
				*out_data = static_cast<void const*>(m_mutableAllocations.m_committed[startIdx].data());
			}
		}
		break;
		case Buffer::AllocationType::Immutable:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				SEAssert(startIdx < m_immutableAllocations.m_committed.size(), "Invalid startIdx");
				*out_data = static_cast<void const*>(&m_immutableAllocations.m_committed[startIdx]);
			}
		}
		break;
		case Buffer::AllocationType::SingleFrame:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				SEAssert(startIdx < m_singleFrameAllocations.m_committed.size(), "Invalid startIdx");
				*out_data = static_cast<void const*>(&m_singleFrameAllocations.m_committed[startIdx]);
			}
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}
	}


	void BufferAllocator::Deallocate(Handle uniqueID)
	{
		Buffer::AllocationType bufferAlloc = re::Buffer::AllocationType::AllocationType_Invalid;
		uint32_t startIdx = -1;
		uint32_t numBytes = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& buffer = m_handleToTypeAndByteIndex.find(uniqueID);
			SEAssert(buffer != m_handleToTypeAndByteIndex.end(), "Cannot deallocate a buffer that does not exist");

			bufferAlloc = buffer->second.m_allocationType;
			startIdx = buffer->second.m_startIndex;
			numBytes = buffer->second.m_numBytes;
		}

		// Add our buffer to the deferred deletion queue, then erase the pointer from our allocation list
		auto ProcessErasure = [&](IAllocation& allocation)
			{
				AddToDeferredDeletions(m_currentFrameNum, allocation.m_handleToPtr.at(uniqueID));

				// Erase the buffer from our allocations:
				{
					std::lock_guard<std::recursive_mutex> lock(allocation.m_mutex);
					allocation.m_handleToPtr.erase(uniqueID);
				}

				allocation.m_currentAllocationsByteSize -= numBytes;
			};
		switch (bufferAlloc)
		{
		case Buffer::AllocationType::Mutable:
		{
			ProcessErasure(m_mutableAllocations);			
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				m_mutableAllocations.m_partialCommits.erase(uniqueID);
			}
		}
		break;
		case Buffer::AllocationType::Immutable:
		{
			ProcessErasure(m_immutableAllocations);
		}
		break;
		case Buffer::AllocationType::SingleFrame:
		{
			ProcessErasure(m_singleFrameAllocations);
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		// Remove the handle from our map:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);

			auto const& buffer = m_handleToTypeAndByteIndex.find(uniqueID);
			m_handleToTypeAndByteIndex.erase(buffer);
		}

		// Finally, free any permanently committed memory:
		if (bufferAlloc == Buffer::AllocationType::Mutable)
		{
			{
				std::scoped_lock lock(m_mutableAllocations.m_mutex, m_handleToTypeAndByteIndexMutex);

				const size_t idxToReplace = startIdx;
				const size_t idxToMove = m_mutableAllocations.m_committed.size() - 1;

				SEAssert(idxToReplace <= idxToMove &&
					idxToReplace < m_mutableAllocations.m_committed.size(),
					"Invalid index to move or replace");

				if (idxToReplace != idxToMove)
				{
					m_mutableAllocations.m_committed[idxToReplace] = std::move(m_mutableAllocations.m_committed[idxToMove]);

					// Update the records for the entry that we moved. This is a slow linear search through an
					// unordered map, but permanent buffers should be deallocated very infrequently
					bool didUpdate = false;
					for (auto& entry : m_handleToTypeAndByteIndex)
					{
						if (entry.second.m_allocationType == bufferAlloc && entry.second.m_startIndex == idxToMove)
						{
							entry.second.m_startIndex = util::CheckedCast<uint32_t>(idxToReplace);
							didUpdate = true;
							break;
						}
					}
					SEAssert(didUpdate, "Failed to find entry to update");
				}
				m_mutableAllocations.m_committed.pop_back();
			}
		}
	}


	// Buffer dirty data
	void BufferAllocator::BufferData()
	{
		SEBeginCPUEvent("re::BufferAllocator::BufferData");
		{
			std::scoped_lock dirtyLock(m_dirtyBuffersMutex, m_dirtyBuffersForPlatformUpdateMutex);

			// We keep mutable buffers committed within m_numFramesInFlight in the dirty list to ensure they're
			// kept up to date
			std::unordered_set<Handle> dirtyMutableBuffers;

			const uint8_t curFrameHeapOffsetFactor = m_currentFrameNum % m_numFramesInFlight; // Only used for mutable buffers

			auto BufferTemporaryData = [&](IAllocation& allocation, Handle currentHandle)
				{
					{
						std::lock_guard<std::recursive_mutex> lock(allocation.m_mutex);

						SEAssert(allocation.m_handleToPtr.contains(currentHandle), "Buffer is not registered");
						re::Buffer const* currentBuffer = allocation.m_handleToPtr.at(currentHandle).get();

						SEAssert(currentBuffer->GetPlatformParams()->m_isCommitted,
							"Trying to buffer a buffer that has not had an initial commit made");

						platform::Buffer::Update(*currentBuffer, curFrameHeapOffsetFactor, 0, 0);
					}

					// Invalidate the commit metadata:
					{
						std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);
						m_handleToTypeAndByteIndex.at(currentHandle).m_startIndex = k_invalidCommitValue;
					}
				};

			for (Handle currentHandle : m_dirtyBuffers)
			{
				Buffer::AllocationType bufferAlloc = Buffer::AllocationType::AllocationType_Invalid;
				{
					std::lock_guard<std::recursive_mutex> lock(m_handleToTypeAndByteIndexMutex);
					bufferAlloc = m_handleToTypeAndByteIndex.find(currentHandle)->second.m_allocationType;
				}

				// NOTE: Getting the mutexes in this block below is a potential deadlock risk as we already hold the
				// m_dirtyBuffersMutex. It's safe for now, but leaving this comment here in case things change...
				switch (bufferAlloc)
				{
				case Buffer::AllocationType::Mutable:
				{
					std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

					SEAssert(m_mutableAllocations.m_handleToPtr.contains(currentHandle), "Buffer is not registered");
					re::Buffer const* currentBuffer = m_mutableAllocations.m_handleToPtr.at(currentHandle).get();

					SEAssert(currentBuffer->GetPlatformParams()->m_isCommitted,
						"Trying to buffer a buffer that has not had an initial commit made");

					SEAssert(m_mutableAllocations.m_partialCommits.contains(currentHandle),
						"Cannot find mutable buffer, was it ever committed?");

					MutableAllocation::CommitRecord& commitRecords =
						m_mutableAllocations.m_partialCommits.at(currentHandle);

					auto partialCommit = commitRecords.begin();
					while (partialCommit != commitRecords.end())
					{
						switch (currentBuffer->GetBufferParams().m_memPoolPreference)
						{
						case re::Buffer::MemoryPoolPreference::Default:
						{
							m_dirtyBuffersForPlatformUpdate.emplace_back(PlatformCommitMetadata
								{
									.m_buffer = currentBuffer,
									.m_baseOffset = partialCommit->m_baseOffset,
									.m_numBytes = partialCommit->m_numBytes,
								});
						}
						break;
						case re::Buffer::MemoryPoolPreference::Upload:
						{
							platform::Buffer::Update(
								*currentBuffer,
								curFrameHeapOffsetFactor,
								partialCommit->m_baseOffset,
								partialCommit->m_numBytes);
						}
						break;
						default: SEAssertF("Invalid MemoryPoolPreference");
						}

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
							dirtyMutableBuffers.emplace(currentHandle); // No-op if the buffer was already recorded
							++partialCommit;
						}
					}
				}
				break;
				case Buffer::AllocationType::Immutable:
				{
					SEAssert(m_immutableAllocations.m_handleToPtr.contains(currentHandle), "Buffer is not registered");
					re::Buffer const* currentBuffer = m_immutableAllocations.m_handleToPtr.at(currentHandle).get();

					switch (currentBuffer->GetBufferParams().m_memPoolPreference)
					{
					case re::Buffer::MemoryPoolPreference::Default:
					{
						// If CPU writes are disabled, our buffer will need to be updated via a command list. Record
						// the update metadata, we'll process these cases in a single batch at the end
						m_dirtyBuffersForPlatformUpdate.emplace_back(PlatformCommitMetadata
							{
								.m_buffer = currentBuffer,
								.m_baseOffset = 0,
								.m_numBytes = currentBuffer->GetTotalBytes(),
							});
					}
					break;
					case re::Buffer::MemoryPoolPreference::Upload:
					{
						BufferTemporaryData(m_immutableAllocations, currentHandle);
					}
					break;
					default: SEAssertF("Invalid MemoryPoolPreference");
					}
				}
				break;
				case Buffer::AllocationType::SingleFrame:
				{
					BufferTemporaryData(m_singleFrameAllocations, currentHandle);
				}
				break;
				default: SEAssertF("Invalid AllocationType");
				}
			}

			// Swap in our dirty list for the next frame
			m_dirtyBuffers = std::move(dirtyMutableBuffers);
		}

		// Perform any platform-specific buffering (e.g. update buffers that do not have CPU writes enabled)
		{
			std::lock_guard<std::mutex> lock(m_dirtyBuffersForPlatformUpdateMutex);
			BufferDataPlatform();
			m_dirtyBuffersForPlatformUpdate.clear();
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
			
			// Increment the write index
			m_writeIdx = (m_writeIdx + 1) % m_numFramesInFlight;

			// Reset the stack base index back to 0 for each type of shared buffer:
			for (uint8_t dataType = 0; dataType < re::Buffer::Type::Type_Count; dataType++)
			{
				m_bufferBaseIndexes[dataType].store(0);
			}
		}
	}


	void BufferAllocator::EndFrame()
	{
		SEBeginCPUEvent("re::BufferAllocator::EndFrame");

		// Clear single-frame allocations:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);		

			// Calling Destroy() on our Buffer recursively calls BufferAllocator::Deallocate, which
			// erases an entry from m_singleFrameAllocations.m_handleToPtr. Thus, we can't use an iterator as it'll be
			// invalidated. Instead, we just loop until it's empty
			while (!m_singleFrameAllocations.m_handleToPtr.empty())
			{
				SEAssert(m_singleFrameAllocations.m_handleToPtr.begin()->second.use_count() == 1,
					std::format("Trying to deallocate a single frame buffer \"{}\", but there is still a live "
						"shared_ptr. Is something holding onto a single frame buffer beyond the frame lifetime? Or, "
						"has a batch been added to a stage, but the stage is not added to the pipeline (thus has not "
						"been cleared)?",
						m_singleFrameAllocations.m_handleToPtr.begin()->second->GetName()).c_str());

				m_singleFrameAllocations.m_handleToPtr.begin()->second->Destroy();
			}

			m_singleFrameAllocations.m_handleToPtr.clear();
			m_singleFrameAllocations.m_committed.clear();

			SEAssert(m_singleFrameAllocations.m_currentAllocationsByteSize == 0,
				"Single frame temporary deallocations are out of sync");
		}

		// Clear immutable allocations: We only write this data exactly once, no point keeping it around
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			m_immutableAllocations.m_committed.clear();
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


	uint32_t BufferAllocator::AdvanceBaseIdx(re::Buffer::Type dataType, uint32_t alignedSize)
	{
		// Atomically advance the stack base index for the next call, and return the base index for the current one
		const uint32_t allocationBaseIdx = m_bufferBaseIndexes[dataType].fetch_add(alignedSize);

		SEAssert(allocationBaseIdx + alignedSize <= k_fixedAllocationByteSize,
			"Allocation is out of bounds. Consider increasing k_fixedAllocationByteSize");

		return allocationBaseIdx;
	}
}