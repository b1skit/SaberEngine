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
		, m_singleFrameGPUWriteIdx(0)
		, m_currentFrameNum(k_invalidFrameNum)
		, m_isValid(false)
	{
		// We maintain N stack base indexes for each Type; Initialize them to 0
		for (uint8_t allocationPoolIdx = 0; allocationPoolIdx < AllocationPool_Count; allocationPoolIdx++)
		{
			m_bufferBaseIndexes[allocationPoolIdx].store(0);
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

		// Register the buffer allocator as the global allocator for buffers:
		re::Buffer::s_bufferAllocator = this;
	}


	void BufferAllocator::Initialize(uint64_t currentFrame)
	{
		m_currentFrameNum = currentFrame;
		m_numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();
		m_isValid = true;

		m_singleFrameGPUWriteIdx = 0;
	}


	BufferAllocator::~BufferAllocator()
	{
		SEAssert(!IsValid(),
			"Buffer allocator destructor called before Destroy(). The buffer allocator must "
			"be manually destroyed (i.e. in the api-specific Context::Destroy())");

		re::Buffer::s_bufferAllocator = nullptr;
	}
	

	void BufferAllocator::Destroy()
	{
		m_dirtyBuffers.clear();

		{
			std::scoped_lock lock(
				m_handleToCommitMetadataMutex,
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

			SEAssert(m_mutableAllocations.m_handleToPtr.empty() && 
				m_immutableAllocations.m_handleToPtr.empty() &&
				m_singleFrameAllocations.m_handleToPtr.empty(),
				"Some buffers have not been destroyed yet");

			SEAssert(m_mutableAllocations.m_currentAllocationsByteSize == 0 &&
				m_immutableAllocations.m_currentAllocationsByteSize == 0 &&
				m_singleFrameAllocations.m_currentAllocationsByteSize == 0,
				"Deallocations and tracking data are out of sync");

			SEAssert(m_handleToCommitMetadata.empty(), "Handle to type and byte map should be cleared by now");
		}

		m_isValid = false;
	}


	void BufferAllocator::Register(std::shared_ptr<re::Buffer> const& buffer, uint32_t numBytes)
	{
		SEBeginCPUEvent("BufferAllocator::Register");

		SEAssert(!buffer->GetPlatformObject()->m_isCreated,
			"Buffer is already marked as created. This should not be possible");

		const Buffer::StagingPool stagingPool = buffer->GetStagingPool();
		SEAssert(stagingPool != re::Buffer::StagingPool::StagingPool_Invalid, "Invalid AllocationType");

		const Handle uniqueID = buffer->GetUniqueID();

		auto RecordHandleToPointer = [&](IAllocation& allocation)
			{
				std::lock_guard<std::recursive_mutex> lock(allocation.m_mutex);
				SEAssert(!allocation.m_handleToPtr.contains(uniqueID), "Buffer is already registered");
				allocation.m_handleToPtr.emplace(uniqueID, buffer);
			};


		switch (stagingPool)
		{
		case Buffer::StagingPool::Permanent:
		{
			RecordHandleToPointer(m_mutableAllocations);
		}
		break;
		case Buffer::StagingPool::Temporary:
		case Buffer::StagingPool::None:
		{
			switch (buffer->GetLifetime())
			{
			case re::Lifetime::Permanent:
			{
				RecordHandleToPointer(m_immutableAllocations);
			}
			break;
			case re::Lifetime::SingleFrame:
			{
				RecordHandleToPointer(m_singleFrameAllocations);
			}
			break;
			default: SEAssertF("Invalid lifetime");
			}

			// Unstaged buffers never commit any data, so we must add them to the dirty buffers list here to ensure
			// they're created (i.e. on the main render thread as required by OpenGL)
			if (stagingPool == Buffer::StagingPool::None)
			{
				std::lock_guard<std::mutex> lock(m_dirtyBuffersMutex);

				m_dirtyBuffers.emplace(buffer);
			}
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		// Record the initial commit metadata:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToCommitMetadataMutex);

			SEAssert(m_handleToCommitMetadata.find(uniqueID) == m_handleToCommitMetadata.end(),
				"A buffer with this handle has already been added");

			// Update our ID -> data tracking table:
			m_handleToCommitMetadata.insert(
				{ uniqueID, CommitMetadata{stagingPool, buffer->GetLifetime(), k_invalidStartIdx, numBytes} });
		}

		SEEndCPUEvent();
	}


	uint32_t BufferAllocator::Allocate(
		Handle uniqueID, uint32_t totalBytes, Buffer::StagingPool stagingPool, re::Lifetime bufferLifetime)
	{
		SEBeginCPUEvent("BufferAllocator::Allocate");

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		uint32_t startIdx = k_invalidStartIdx;

		auto UpdateAllocationTracking = [&totalBytes](IAllocation& allocation)
			{
				// Note: IAllocation::m_mutex is already locked

				allocation.m_totalAllocations++;
				allocation.m_totalAllocationsByteSize += totalBytes;
				allocation.m_currentAllocationsByteSize += totalBytes;
				allocation.m_maxAllocations =
					std::max(allocation.m_maxAllocations, util::CheckedCast<uint32_t>(allocation.m_handleToPtr.size()));
				allocation.m_maxAllocationsByteSize =
					std::max(allocation.m_maxAllocationsByteSize, allocation.m_currentAllocationsByteSize);
			};

		switch (stagingPool)
		{
		case Buffer::StagingPool::Permanent:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				startIdx = util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed.size());
				m_mutableAllocations.m_committed.emplace_back(totalBytes, 0); // Add a new zero-filled vector<uint8_t>

				UpdateAllocationTracking(m_mutableAllocations);
			}
		}
		break;
		case Buffer::StagingPool::Temporary:
		{
			switch (bufferLifetime)
			{
			case re::Lifetime::Permanent:
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);

				startIdx = util::CheckedCast<uint32_t>(m_immutableAllocations.m_committed.size());

				m_immutableAllocations.m_committed.resize(
					util::CheckedCast<uint32_t>(m_immutableAllocations.m_committed.size() + totalBytes),
					0);

				UpdateAllocationTracking(m_immutableAllocations);
			}
			break;
			case re::Lifetime::SingleFrame:
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

				startIdx = util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size());

				m_singleFrameAllocations.m_committed.resize(
					util::CheckedCast<uint32_t>(m_singleFrameAllocations.m_committed.size() + totalBytes),
					0);

				UpdateAllocationTracking(m_singleFrameAllocations);
			}
			break;
			default: SEAssertF("Invalid lifetime");
			}
		}
		break;
		case Buffer::StagingPool::None:
		{
			// Do nothing
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		// Store the starting data index in our ID -> metadata tracking table:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToCommitMetadataMutex);

			auto entryItr = m_handleToCommitMetadata.find(uniqueID);

			SEAssert(entryItr != m_handleToCommitMetadata.end(), "A buffer with this handle has not been registered");
			SEAssert(entryItr->second.m_startIndex == k_invalidStartIdx, "Buffer has already been allocated");

			m_handleToCommitMetadata.at(uniqueID).m_startIndex = startIdx;
		}

		SEEndCPUEvent();

		return startIdx;
	}


	void BufferAllocator::Stage(Handle uniqueID, void const* data)
	{
		SEBeginCPUEvent("BufferAllocator::Stage");

		uint32_t startIdx;
		uint32_t totalBytes;
		Buffer::StagingPool stagingPool;
		re::Lifetime bufferLifetime;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToCommitMetadataMutex);

			auto const& result = m_handleToCommitMetadata.find(uniqueID);

			SEAssert(result != m_handleToCommitMetadata.end(), "Buffer with this ID has not been allocated");

			startIdx = result->second.m_startIndex;
			totalBytes = result->second.m_totalBytes;
			stagingPool = result->second.m_stagingPool;
			bufferLifetime = result->second.m_bufferLifetime;
		}

		// If it's our first commit, allocate first:
		if (startIdx == k_invalidStartIdx)
		{
			startIdx = Allocate(uniqueID, totalBytes, stagingPool, bufferLifetime);
		}


		// Copy the data to our pre-allocated region.
		switch (stagingPool)
		{
		case Buffer::StagingPool::Permanent:
		{
			StageMutable(uniqueID, data, totalBytes, 0); // Internally adds the buffer to m_dirtyBuffers
		}
		break;
		case Buffer::StagingPool::Temporary:
		{
			std::shared_ptr<re::Buffer> dirtyBuffer;
			switch (bufferLifetime)
			{
			case re::Lifetime::Permanent:
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				void* dest = &m_immutableAllocations.m_committed[startIdx];
				memcpy(dest, data, totalBytes);

				dirtyBuffer = m_immutableAllocations.m_handleToPtr.at(uniqueID).lock();
			}
			break;
			case re::Lifetime::SingleFrame:
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				void* dest = &m_singleFrameAllocations.m_committed[startIdx];
				memcpy(dest, data, totalBytes);

				dirtyBuffer = m_singleFrameAllocations.m_handleToPtr.at(uniqueID).lock();
			}
			break;
			default: SEAssertF("Invalid lifetime");
			}
			SEAssert(dirtyBuffer != nullptr, "Failed to convert weak to shared_ptr: Buffer leaked?");

			// Add the committed buffer to our dirty list, so we can buffer the data when required
			{
				std::lock_guard<std::mutex> lock(m_dirtyBuffersMutex);

				m_dirtyBuffers.emplace(dirtyBuffer);
			}
		}
		break;
		case Buffer::StagingPool::None:
		{
			// Do nothing
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		SEEndCPUEvent();
	}


	void BufferAllocator::StageMutable(Handle uniqueID, void const* data, uint32_t numBytes, uint32_t dstBaseByteOffset)
	{
		SEBeginCPUEvent("BufferAllocator::StageMutable");

		SEAssert(numBytes > 0, "0 bytes is only valid for signalling the Buffer::Update to update all bytes");

		uint32_t startIdx;
		uint32_t totalBytes;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToCommitMetadataMutex);

			auto const& result = m_handleToCommitMetadata.find(uniqueID);

			SEAssert(result != m_handleToCommitMetadata.end(), "Buffer with this ID has not been allocated");
			SEAssert(result->second.m_stagingPool == re::Buffer::StagingPool::Permanent &&
				result->second.m_bufferLifetime == re::Lifetime::Permanent,
				"Can only partially commit to mutable buffers");
			
			startIdx = result->second.m_startIndex;
			totalBytes = result->second.m_totalBytes;

			SEAssert(numBytes <= totalBytes, "Trying to commit more data than is allocated");
		}

		// If it's our first commit, allocate first:
		if (startIdx == k_invalidStartIdx)
		{
			startIdx = Allocate(uniqueID, totalBytes, Buffer::StagingPool::Permanent, re::Lifetime::Permanent);
		}

		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			SEAssert(startIdx < m_mutableAllocations.m_committed.size() &&
				totalBytes == util::CheckedCast<uint32_t>(m_mutableAllocations.m_committed[startIdx].size()),
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
						return std::upper_bound( // Find first element *greater than*
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

				const MutableAllocation::PartialCommit newCommit{
					.m_baseOffset = dstBaseByteOffset,
					.m_numBytes = numBytes,
					.m_numRemainingUpdates = m_numFramesInFlight };

				auto prev = commitRecord.insert(GetSortedInsertionPointItr(newCommit), newCommit);
				if (prev != commitRecord.begin())
				{
					auto previous = std::prev(prev);
					if (previous->m_baseOffset + previous->m_numBytes >= prev->m_baseOffset)
					{
						--prev; // Prior element overlaps: Start our search from there
					}
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
							const MutableAllocation::PartialCommit lowerSplit{
									.m_baseOffset = prev->m_baseOffset,
									.m_numBytes = (current->m_baseOffset - prev->m_baseOffset),
									.m_numRemainingUpdates = prev->m_numRemainingUpdates};

							const MutableAllocation::PartialCommit upperSplit{
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

//#define VALIDATE_COMMITS
#if defined(VALIDATE_COMMITS)
			for (auto itr = commitRecord.begin(); itr != commitRecord.end(); ++itr)
			{
				auto next = std::next(itr);

				SEAssert(next == commitRecord.end() ||
					itr->m_baseOffset < next->m_baseOffset && // Not overlapping
					(itr->m_baseOffset + itr->m_numBytes < next->m_baseOffset || // Not contiguous
						(itr->m_baseOffset + itr->m_numBytes == next->m_baseOffset && // Contiguous, but diff. updates
							itr->m_numRemainingUpdates != next->m_numRemainingUpdates)),
					"Commit records are out of sync");
			}
#endif
		}


		// Add the mutable buffer to our dirty list, so we can buffer the data when required
		{
			std::lock_guard<std::mutex> lock(m_dirtyBuffersMutex);

			m_dirtyBuffers.emplace(m_mutableAllocations.m_handleToPtr.at(uniqueID)); // No-op if the Buffer is already recorded
		}

		SEEndCPUEvent();
	}


	void BufferAllocator::GetData(Handle uniqueID, void const** out_data) const
	{
		SEBeginCPUEvent("BufferAllocator::GetData");

		Buffer::StagingPool stagingPool;
		re::Lifetime bufferLifetime;
		uint32_t startIdx = -1;
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToCommitMetadataMutex);

			auto const& result = m_handleToCommitMetadata.find(uniqueID);
			SEAssert(result != m_handleToCommitMetadata.end(), "Buffer with this ID has not been allocated");

			stagingPool = result->second.m_stagingPool;
			bufferLifetime = result->second.m_bufferLifetime;
			startIdx = result->second.m_startIndex;
		}

		// Note: This is not thread safe, as the pointer will become stale if m_committed is resized. This should be
		// fine though, as the BufferAllocator is simply a temporary staging ground for data about to be copied
		// to GPU heaps. Copies in/resizing should all be done before this function is ever called
		switch (stagingPool)
		{
		case Buffer::StagingPool::Permanent:
		{
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				SEAssert(startIdx < m_mutableAllocations.m_committed.size(), "Invalid startIdx");
				*out_data = static_cast<void const*>(m_mutableAllocations.m_committed[startIdx].data());
			}
		}
		break;
		case Buffer::StagingPool::Temporary:
		{
			switch (bufferLifetime)
			{
			case re::Lifetime::Permanent:
			{
				std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
				SEAssert(startIdx < m_immutableAllocations.m_committed.size(), "Invalid startIdx");
				*out_data = static_cast<void const*>(&m_immutableAllocations.m_committed[startIdx]);
			}
			break;
			case re::Lifetime::SingleFrame:
			{
				std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
				SEAssert(startIdx < m_singleFrameAllocations.m_committed.size(), "Invalid startIdx");
				*out_data = static_cast<void const*>(&m_singleFrameAllocations.m_committed[startIdx]);
			}
			break;
			default: SEAssertF("Invalid lifetime");
			}
		}
		break;
		case Buffer::StagingPool::None:
		{
			*out_data = nullptr;
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		SEEndCPUEvent();
	}


	void BufferAllocator::Deallocate(Handle uniqueID)
	{
		SEBeginCPUEvent("BufferAllocator::Deallocate");

		Buffer::StagingPool stagingPool = re::Buffer::StagingPool::StagingPool_Invalid;
		re::Lifetime bufferLifetime;
		uint32_t startIdx = std::numeric_limits<uint32_t>::max();
		uint32_t numBytes = std::numeric_limits<uint32_t>::max();
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToCommitMetadataMutex);

			auto const& metadata = m_handleToCommitMetadata.find(uniqueID);
			SEAssert(metadata != m_handleToCommitMetadata.end(), "Cannot deallocate a buffer that does not exist");

			stagingPool = metadata->second.m_stagingPool;
			bufferLifetime = metadata->second.m_bufferLifetime;
			startIdx = metadata->second.m_startIndex;
			numBytes = metadata->second.m_totalBytes;
		}

		// Add our buffer to the deferred deletion queue, then erase the pointer from our allocation list
		auto ProcessErasure = [&](IAllocation& allocation, re::Buffer::StagingPool stagingPool)
			{
				// Erase the buffer from our allocations:
				{
					std::lock_guard<std::recursive_mutex> lock(allocation.m_mutex);
					allocation.m_handleToPtr.erase(uniqueID);
				}

				if (stagingPool != re::Buffer::StagingPool::None)
				{
					SEAssert(allocation.m_currentAllocationsByteSize >= numBytes, "About to underflow");

					allocation.m_currentAllocationsByteSize -= numBytes;
				}
			};

		switch (stagingPool)
		{
		case Buffer::StagingPool::Permanent:
		{
			ProcessErasure(m_mutableAllocations, stagingPool);
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
				m_mutableAllocations.m_partialCommits.erase(uniqueID);
			}
		}
		break;
		case Buffer::StagingPool::Temporary:
		case Buffer::StagingPool::None:
		{
			switch (bufferLifetime)
			{
			case re::Lifetime::Permanent:
			{
				ProcessErasure(m_immutableAllocations, stagingPool);
			}
			break;
			case re::Lifetime::SingleFrame:
			{
				ProcessErasure(m_singleFrameAllocations, stagingPool);
			}
			break;
			default: SEAssertF("Invalid lifetime");
			}
		}
		break;
		default: SEAssertF("Invalid AllocationType");
		}

		// Remove the handle from our map:
		{
			std::lock_guard<std::recursive_mutex> lock(m_handleToCommitMetadataMutex);

			m_handleToCommitMetadata.erase(uniqueID);
		}

		// Finally, free any permanently committed memory:
		if (stagingPool == Buffer::StagingPool::Permanent)
		{
			{
				std::scoped_lock lock(m_mutableAllocations.m_mutex, m_handleToCommitMetadataMutex);

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
					for (auto& entry : m_handleToCommitMetadata)
					{
						if (entry.second.m_stagingPool == stagingPool && entry.second.m_startIndex == idxToMove)
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

		SEEndCPUEvent();
	}


	void BufferAllocator::ResetForNewFrame(uint64_t renderFrameNum)
	{
		SEBeginCPUEvent("BufferAllocator::ResetForNewFrame");

		// Avoid stomping existing data when the BufferAllocator has already been accessed (e.g. during
		// RenderManager::Initialize, before BufferAllocator::BufferData has been called)
		if (renderFrameNum != m_currentFrameNum)
		{
			m_currentFrameNum = renderFrameNum;

			// Increment the single frame GPU resource write index:
			m_singleFrameGPUWriteIdx = (m_singleFrameGPUWriteIdx + 1) % m_numFramesInFlight;

			// Reset the stack base index back to 0 for each type of shared buffer:
			for (uint8_t allocationPoolIdx = 0; allocationPoolIdx < AllocationPool_Count; allocationPoolIdx++)
			{
				m_bufferBaseIndexes[allocationPoolIdx].store(0);
			}
		}

		SEEndCPUEvent();
	}


	void BufferAllocator::CreateBufferPlatformObjects() const
	{
		SEBeginCPUEvent("BufferAllocator::CreateBufferPlatformObjects");

		// Pre-create buffer platform objects:
		{
			std::lock_guard<std::mutex> lock(m_dirtyBuffersMutex);

			for (std::shared_ptr<re::Buffer> const& currentBuffer : m_dirtyBuffers)
			{
				if (!currentBuffer->GetPlatformObject()->m_isCreated)
				{
					platform::Buffer::Create(*currentBuffer);
				}
			}
		}

		SEEndCPUEvent();
	}


	uint8_t BufferAllocator::GetFrameOffsetIndex() const noexcept
	{
		return m_currentFrameNum % m_numFramesInFlight;
	}


	void BufferAllocator::BufferData(uint64_t renderFrameNum)
	{
		SEBeginCPUEvent("re::BufferAllocator::BufferData");

		{
			// This is a blocking call: Lock all of the mutexes (except for m_deferredDeleteQueueMutex, which is
			// indirectly locked when we destroy single frame buffers at the end during ClearTemporaryStaging(), and
			// when we call ClearDeferredDeletions())
			std::scoped_lock lock(
				m_mutableAllocations.m_mutex,
				m_immutableAllocations.m_mutex,
				m_singleFrameAllocations.m_mutex,
				m_handleToCommitMetadataMutex,
				m_dirtyBuffersMutex);

			// Start by resetting all of our indexes etc:
			ResetForNewFrame(renderFrameNum);

			SEBeginCPUEvent("re::BufferAllocator::BufferData: Dirty buffers");

			// We keep mutable buffers committed within m_numFramesInFlight in the dirty list to ensure they're
			// kept up to date
			std::unordered_set<std::shared_ptr<re::Buffer>> stillDirtyMutableBuffers;
			std::vector<PlatformCommitMetadata> dirtyDefaultHeapBuffers;
			std::vector<PlatformCommitMetadata> dirtyUploadHeapBuffers;

			std::vector<decltype(m_handleToCommitMetadata)::iterator> temporaryMetadataToInvalidate;

			auto BufferTemporaryData = [&](Handle currentHandle, re::Buffer* currentBuffer)
				{
					SEAssert(currentBuffer->GetPlatformObject()->m_isCommitted,
						"Trying to buffer a buffer that has not had an initial commit made");

					dirtyUploadHeapBuffers.emplace_back(PlatformCommitMetadata
						{
							.m_buffer = currentBuffer,
							.m_baseOffset = 0,
							.m_numBytes = currentBuffer->GetTotalBytes(),
						});

					temporaryMetadataToInvalidate.emplace_back(m_handleToCommitMetadata.find(currentHandle));
				};


			for (std::shared_ptr<re::Buffer> const& currentBuffer : m_dirtyBuffers)
			{
				// If m_dirtyBuffers is the only thing keeping our Buffer alive, skip the update as the buffer is about
				// to go out of scope
				if (currentBuffer.use_count() == 1)
				{
					continue;
				}

				// Trigger platform creation, if necessary.
				// Note: It is possible we have buffers created *after* the CreateBufferPlatformObjects() call, we must
				// still ensure they're created here:
				if (!currentBuffer->GetPlatformObject()->m_isCreated)
				{
					platform::Buffer::Create(*currentBuffer);
				}

				const Handle currentHandle = currentBuffer->GetUniqueID();

				SEAssert(m_handleToCommitMetadata.contains(currentHandle), "Failed to find current handle");

				CommitMetadata const& commitMetadata = m_handleToCommitMetadata.at(currentHandle);

				const Buffer::StagingPool stagingPool = commitMetadata.m_stagingPool;
				const re::Lifetime bufferLifetime = commitMetadata.m_bufferLifetime;

				// NOTE: Getting the mutexes in this block below is a potential deadlock risk as we already hold the
				// m_dirtyBuffersMutex. It's safe for now, but leaving this comment here in case things change...
				switch (stagingPool)
				{
				case Buffer::StagingPool::Permanent:
				{
					SEAssert(m_mutableAllocations.m_handleToPtr.contains(currentHandle), "Buffer is not registered");

					SEAssert(currentBuffer->GetPlatformObject()->m_isCommitted,
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
						case re::Buffer::DefaultHeap:
						{
							dirtyDefaultHeapBuffers.emplace_back(PlatformCommitMetadata
								{
									.m_buffer = currentBuffer.get(),
									.m_baseOffset = partialCommit->m_baseOffset,
									.m_numBytes = partialCommit->m_numBytes,
								});
						}
						break;
						case re::Buffer::UploadHeap:
						{
							dirtyUploadHeapBuffers.emplace_back(PlatformCommitMetadata
								{
									.m_buffer = currentBuffer.get(),
									.m_baseOffset = partialCommit->m_baseOffset,
									.m_numBytes = partialCommit->m_numBytes,
								});
						}
						break;
						default: SEAssertF("Invalid MemoryPoolPreference");
						}

						// Decrement the remaining updates counter: If 0, the commit has been fully propogated to 
						// all  buffers and we can remove it
						partialCommit->m_numRemainingUpdates--;
						if (partialCommit->m_numRemainingUpdates == 0)
						{
							partialCommit = commitRecords.erase(partialCommit); // Returns itr after the erased element
						}
						else
						{
							stillDirtyMutableBuffers.emplace(currentBuffer); // No-op if the buffer was already recorded
							++partialCommit;
						}
					}
				}
				break;
				case Buffer::StagingPool::Temporary:
				{
					switch (bufferLifetime)
					{
					case re::Lifetime::Permanent:
					{
						SEAssert(m_immutableAllocations.m_handleToPtr.contains(currentHandle), "Buffer is not registered");

						switch (currentBuffer->GetBufferParams().m_memPoolPreference)
						{
						case re::Buffer::DefaultHeap:
						{
							// If CPU writes are disabled, our buffer will need to be updated via a command list. Record
							// the update metadata, we'll process these cases in a single batch at the end
							dirtyDefaultHeapBuffers.emplace_back(PlatformCommitMetadata
								{
									.m_buffer = currentBuffer.get(),
									.m_baseOffset = 0,
									.m_numBytes = currentBuffer->GetTotalBytes(),
								});
						}
						break;
						case re::Buffer::UploadHeap:
						{
							BufferTemporaryData(currentHandle, currentBuffer.get());
						}
						break;
						default: SEAssertF("Invalid MemoryPoolPreference");
						}
					}
					break;
					case re::Lifetime::SingleFrame:
					{
						BufferTemporaryData(currentHandle, currentBuffer.get());
					}
					break;
					default: SEAssertF("Invalid lifetime");
					}
				}
				break;
				case Buffer::StagingPool::None:
				{
					//
				}
				break;
				default: SEAssertF("Invalid AllocationType");
				}
			}

			// Swap in our dirty list for the next frame
			m_dirtyBuffers = std::move(stillDirtyMutableBuffers);

			SEEndCPUEvent(); // "re::BufferAllocator::BufferData: Dirty buffers"

			// Trigger platform buffering:
			BufferDataPlatform(std::move(dirtyDefaultHeapBuffers), std::move(dirtyUploadHeapBuffers));

			// We're done! Clear everything for the next round:
			SEBeginCPUEvent("re::BufferAllocator: Clear temp staging and deferred deletions");

			// Book keeping: Invalidate m_startIndex now that we've buffered temporary buffer data:
			for (auto const& temporaryItr : temporaryMetadataToInvalidate)
			{
				temporaryItr->second.m_startIndex = k_invalidCommitValue;
			}

			ClearTemporaryStaging();
			SEEndCPUEvent();
		}

		SEEndCPUEvent(); // "re::BufferAllocator::BufferData"
	}


	void BufferAllocator::BufferDataPlatform(
		std::vector<PlatformCommitMetadata>&& defaultHeapCommits,
		std::vector<PlatformCommitMetadata>&& uploadHeapCommits)
	{
		// Perform any platform-specific buffering (e.g. update buffers that do not have CPU writes enabled)
		SEBeginCPUEvent("re::BufferAllocator::BufferDataPlatform");

		// Optimization: We sort the dirty buffers, and then merge contiguous updates
		auto MergeContiguousCommits = [](std::vector<PlatformCommitMetadata>& dirtyBuffers)
			-> std::vector<PlatformCommitMetadata>
			{
				std::sort(
					dirtyBuffers.begin(),
					dirtyBuffers.end(),
					[](PlatformCommitMetadata const& a, PlatformCommitMetadata const& b)
					{
						if (a.m_buffer->GetUniqueID() == b.m_buffer->GetUniqueID())
						{
							return a.m_baseOffset < b.m_baseOffset;
						}
						return a.m_buffer->GetUniqueID() < b.m_buffer->GetUniqueID();
					});

				// Merge contiguous commits into a single call:
				std::vector<PlatformCommitMetadata> mergedDirtyBuffers;
				mergedDirtyBuffers.reserve(dirtyBuffers.size());
				auto itr = dirtyBuffers.begin();
				while (itr != dirtyBuffers.end())
				{
					auto nextItr = std::next(itr);

					// Nothing to merge:
					if (nextItr == dirtyBuffers.end() ||
						itr->m_buffer->GetUniqueID() != nextItr->m_buffer->GetUniqueID() ||
						itr->m_baseOffset + itr->m_numBytes < nextItr->m_baseOffset)
					{
						mergedDirtyBuffers.emplace_back(*itr);
					}
					else // Merge neighboring commits:
					{
						SEAssert(itr->m_buffer->GetUniqueID() == nextItr->m_buffer->GetUniqueID() &&
							itr->m_baseOffset + itr->m_numBytes == nextItr->m_baseOffset,
							"Iterators are out of sync");

						mergedDirtyBuffers.emplace_back(PlatformCommitMetadata{
							.m_buffer = itr->m_buffer,
							.m_baseOffset = itr->m_baseOffset,
							.m_numBytes = itr->m_numBytes + nextItr->m_numBytes, });
						++itr; // Ensure we double-increment, as we just merged the current and next records
					}
					++itr;
				}
				return mergedDirtyBuffers;
			};


		const uint8_t frameOffsetIdx = GetFrameOffsetIndex();

		BufferDefaultHeapDataPlatform(MergeContiguousCommits(defaultHeapCommits), frameOffsetIdx);

		for (auto const& commit : MergeContiguousCommits(uploadHeapCommits))
		{
			platform::Buffer::Update(*commit.m_buffer, frameOffsetIdx, commit.m_baseOffset, commit.m_numBytes);
		}

		SEEndCPUEvent(); // "re::BufferAllocator::BufferDataPlatform"
	}


	void BufferAllocator::ClearTemporaryStaging()
	{
		// NOTE: We hold a scoped lock of (almost) all mutexes before calling this

		SEBeginCPUEvent("re::BufferAllocator::ClearTemporaryStaging");

		m_singleFrameAllocations.m_handleToPtr.clear();
		m_singleFrameAllocations.m_committed.clear();

		// Clear immutable allocations: We only write this data exactly once, no point keeping it around
		m_immutableAllocations.m_committed.clear();

		SEEndCPUEvent();
	}


	uint32_t BufferAllocator::AdvanceBaseIdx(AllocationPool allocationPool, uint32_t alignedSize)
	{
		// Atomically advance the stack base index for the next call, and return the base index for the current one
		const uint32_t allocationBaseIdx = m_bufferBaseIndexes[allocationPool].fetch_add(alignedSize);

		SEAssert(allocationBaseIdx + alignedSize <= k_sharedSingleFrameAllocationByteSize,
			"Allocation is out of bounds. Consider increasing k_sharedSingleFrameAllocationByteSize");

		return allocationBaseIdx;
	}
}