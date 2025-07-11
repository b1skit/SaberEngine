// © 2025 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchBuilder.h"
#include "BatchHandle.h"
#include "BatchPool.h"

#include "Core/Assert.h"
#include "Core/Logger.h"
#include "Core/ProfilingMarkers.h"


namespace
{
	inline void PoolIndexToPageLocalIndexes(
		gr::PoolIndex poolIndex, uint32_t& pageIndexOut, uint32_t& localIndexOut) noexcept
	{
		pageIndexOut = poolIndex / gr::BatchPoolPage::k_pageSize;
		localIndexOut = poolIndex % gr::BatchPoolPage::k_pageSize;
	}
}

namespace gr
{
	BatchPoolPage::BatchPoolPage(uint32_t baseIndex, uint8_t numFramesInFlight) noexcept
		: m_batches{ gr::Batch() }
		, m_baseIndex(baseIndex)
		, m_numFramesInFlight(numFramesInFlight)
	{
		m_freeIndexes.reserve(k_pageSize);
		for (uint32_t i = 0; i < k_pageSize; ++i)
		{
			m_freeIndexes.push_back(i);
			m_batchRefCounts[i].m_value.store(0, std::memory_order_relaxed); // Initialize ref counts to 0
		}
	}


	void BatchPoolPage::Update(
		uint64_t currentFrameNum, std::unordered_map<util::HashKey, PoolIndex>& batchHashToIndexMap) noexcept
	{
		SEBeginCPUEvent("BatchPoolPage::Update");

		ProcessDeferredDeletes(currentFrameNum, batchHashToIndexMap);

		SEEndCPUEvent(); // "BatchPoolPage::Update"
	}


	void BatchPoolPage::ProcessDeferredDeletes(
		uint64_t currentFrameNum, std::unordered_map<util::HashKey, PoolIndex>& batchHashToIndexMap)
	{
		SEBeginCPUEvent("BatchPoolPage::ProcessDeferredDeletes");

		{
			std::lock_guard<std::mutex> lock(m_pageMutex);

			while (!m_deferredDeletes.empty() &&
				m_deferredDeletes.front().second + m_numFramesInFlight < currentFrameNum)
			{
				const uint32_t localIndex = m_deferredDeletes.front().first;

				if (m_batchRefCounts[localIndex].m_value.load() == 0 &&
					m_batches[localIndex].IsValid())
				{
					SEAssert(batchHashToIndexMap.contains(m_batches[localIndex].GetDataHash()),
						"batchHash not found, this should not be possible");

					// Update the batch hash to index map for the BatchPool:
					batchHashToIndexMap.erase(m_batches[localIndex].GetDataHash());

					// Destroy the batch and return the index to the free pool:
					m_batches[localIndex].Destroy();
					m_freeIndexes.push_back(localIndex);
				}

				m_deferredDeletes.pop();
			}
		}

		SEEndCPUEvent(); // "BatchPoolPage::ProcessDeferredDeletes"
	}


	void BatchPoolPage::Destroy(std::unordered_map<util::HashKey, PoolIndex>& batchHashToIndexMap)
	{
		ProcessDeferredDeletes(std::numeric_limits<uint64_t>::max(), batchHashToIndexMap);

		{
			std::lock_guard<std::mutex> lock(m_pageMutex);

			SEAssert(m_freeIndexes.size() == k_pageSize, "Free indexes list is missing elements");

#if defined(_DEBUG)
			for (auto const& batch : m_batches)
			{
				SEAssert(!batch.IsValid(),
					"BatchPoolPage is being destroyed, but some batches are still valid. This is unexpected.");
			}
			for (auto const& refCount : m_batchRefCounts)
			{
				SEAssert(refCount.m_value.load(std::memory_order_relaxed) == 0,
					"BatchPoolPage is being destroyed, but some batches have a non-zero ref count. This is unexpected.");
			}
#endif
		}
	}


	bool BatchPoolPage::AddBatch(gr::Batch&& batch, PoolIndex& outIndex) noexcept
	{
		{
			std::lock_guard<std::mutex> lock(m_pageMutex);

			if (m_freeIndexes.empty())
			{
				return false;
			}

			const uint32_t index = m_freeIndexes.back();
			m_freeIndexes.pop_back();

			SEAssert(!m_batches[index].IsValid(), "Batch at index %u is already valid. This should not happen", index);
			SEAssert(m_batchRefCounts[index].m_value.load(std::memory_order_relaxed) == 0,
				"Batch at index %u has a non-zero ref count. This should not happen", index);

			m_batches[index] = std::move(batch);

			outIndex = m_baseIndex + index; // Convert to global pool index

			return true;
		}
	}


	void BatchPoolPage::AddBatchRef(uint32_t localIndex) noexcept
	{
#if defined(_DEBUG)
		std::lock_guard<std::mutex> lock(m_pageMutex);

		SEAssert(m_batches[localIndex].IsValid(), "Trying to add a ref to an invalid Batch");
#endif

		m_batchRefCounts[localIndex].m_value.fetch_add(1, std::memory_order_relaxed);
	}


	void BatchPoolPage::ReleaseBatch(uint32_t localIndex, uint64_t currentFrameNum) noexcept
	{
		SEAssert(m_batchRefCounts[localIndex].m_value.load() > 0, "About to underflow the counter");

		// We use memory_order_acq_rel here to ensure nothing is reordered
		if (m_batchRefCounts[localIndex].m_value.fetch_sub(1, std::memory_order_acq_rel) == 1) // Free batches with 0 ref
		{
			{
				std::lock_guard<std::mutex> lock(m_pageMutex);

				SEAssert(m_batches[localIndex].IsValid(), "Trying to free an invalid Batch");

				m_deferredDeletes.emplace(std::make_pair(localIndex, currentFrameNum));
			}
		}
	}


	gr::Batch const& BatchPoolPage::GetBatch(uint32_t localIndex) noexcept
	{
		SEAssert(m_batches[localIndex].IsValid(), "Trying to get an invalid Batch");
		return m_batches[localIndex];
	}


	// ---


	BatchPool::BatchPool(uint8_t numFramesInFlight)
		: m_currentFrameNum(0)
		, m_numFramesInFlight(numFramesInFlight)
	{
		SEAssert(m_numFramesInFlight > 0 && m_numFramesInFlight <= 3, "Unexpected number of frames in flight");

		gr::BatchHandle::s_batchPool = this;

		gr::IBatchBuilder<ComputeBatchBuilder>::s_batchPool = this;
		gr::IBatchBuilder<RasterBatchBuilder>::s_batchPool = this;
		gr::IBatchBuilder<RayTraceBatchBuilder>::s_batchPool = this;
	}


	void BatchPool::Destroy()
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_poolMutex);

			LOG("Destroying batch pool (%llu pages)", m_pages.size());

			for (auto& page : m_pages)
			{
				page->Destroy(m_batchHashToIndexMap);
			}
			m_pages.clear();

			SEAssert(m_batchHashToIndexMap.empty(),
				"BatchPool is being destroyed, but m_batchHashToIndexMap is not empty");

			gr::BatchHandle::s_batchPool = nullptr;

			gr::IBatchBuilder<ComputeBatchBuilder>::s_batchPool = nullptr;
			gr::IBatchBuilder<RasterBatchBuilder>::s_batchPool = nullptr;
			gr::IBatchBuilder<RayTraceBatchBuilder>::s_batchPool = nullptr;
		}
	}


	void BatchPool::Update(uint64_t currentFrameNum)
	{
		SEBeginCPUEvent("BatchPool::Update");

		{
			std::unique_lock<std::shared_mutex> writeLock(m_poolMutex);

			m_currentFrameNum = currentFrameNum;

			for (auto& page : m_pages)
			{
				page->Update(m_currentFrameNum, m_batchHashToIndexMap);
			}
		}

		SEEndCPUEvent(); // "BatchPool::Update"
	}


	gr::BatchHandle BatchPool::AddBatch(gr::Batch&& batch, gr::RenderDataID renderDataID) noexcept
	{
		SEBeginCPUEvent("BatchPool::AddBatch");

		const util::HashKey batchHash = batch.GetDataHash();

		auto TryAddBatch = [this, &batchHash, &batch, &renderDataID]() noexcept -> gr::BatchHandle
			{
				// Check if the batch already exists in the pool:
				auto itr = m_batchHashToIndexMap.find(batchHash);
				if (itr != m_batchHashToIndexMap.end())
				{
					// Batch already exists, return the existing handle
					return gr::BatchHandle(itr->second, renderDataID);
				}

				for (auto& page : m_pages)
				{
					PoolIndex poolIndex;
					if (page->AddBatch(std::move(batch), poolIndex))
					{
						SEAssert(!m_batchHashToIndexMap.contains(batchHash),
							"Batch hash already added. This should not be possible");

						m_batchHashToIndexMap.emplace(batchHash, poolIndex);

						uint32_t pageIndex, localIndex;
						PoolIndexToPageLocalIndexes(poolIndex, pageIndex, localIndex);

						return gr::BatchHandle(poolIndex, renderDataID);
					}
				}

				return gr::BatchHandle(); // Invalid
			};

		// Fast path: Add a batch to a page while only holding a read lock on the BatchPool mutex
		{
			std::shared_lock<std::shared_mutex> readLock(m_poolMutex);

			gr::BatchHandle existingBatchHandle = TryAddBatch();
			if (existingBatchHandle.IsValid())
			{
				SEEndCPUEvent(); // "BatchPool::AddBatch"
				return existingBatchHandle;
			}
		}

		PoolIndex poolIndex;
		{
			std::unique_lock<std::shared_mutex> writeLock(m_poolMutex);

			// Retry adding the batch, incase it was added while we were waiting for the lock
			gr::BatchHandle existingBatchHandle = TryAddBatch();
			if (existingBatchHandle.IsValid())
			{
				SEEndCPUEvent(); // "BatchPool::AddBatch"
				return existingBatchHandle;
			}

			// If we made it this far, no page had free space. Create a new page:
			// Note: It's possible a batch was released since we last held the lock, but we'll just accept that and
			// create a new page anyway
			const uint32_t newPageBaseIndex = util::CheckedCast<uint32_t>(m_pages.size()) * BatchPoolPage::k_pageSize;

			m_pages.emplace_back(std::make_unique<BatchPoolPage>(newPageBaseIndex, m_numFramesInFlight));

			LOG("BatchPool: Increased page count to %llu", m_pages.size());
			
			const bool didAdd = m_pages.back()->AddBatch(std::move(batch), poolIndex);
			SEAssert(didAdd, "Failed to add batch to new page. This should not be possible");

			SEAssert(!m_batchHashToIndexMap.contains(batchHash),
				"Batch hash already added. This should not be possible");

			m_batchHashToIndexMap.emplace(batchHash, poolIndex);
		}

		SEEndCPUEvent(); // "BatchPool::AddBatch"
		return gr::BatchHandle(poolIndex, renderDataID); // Avoid deadlock: BatchHandle ctor calls AddBatchRef
	}


	void BatchPool::AddBatchRef(PoolIndex poolIndex) noexcept
	{
		uint32_t pageIndex, localIndex;
		PoolIndexToPageLocalIndexes(poolIndex, pageIndex, localIndex);

		SEAssert(pageIndex < m_pages.size(), "Batch index out of bounds");
		SEAssert(localIndex < BatchPoolPage::k_pageSize, "Local batch index out of bounds");

		{
			std::shared_lock<std::shared_mutex> readLock(m_poolMutex);

			m_pages[pageIndex]->AddBatchRef(localIndex);
		}
	}


	void BatchPool::ReleaseBatch(PoolIndex poolIndex) noexcept
	{
		uint32_t pageIndex, localIndex;
		PoolIndexToPageLocalIndexes(poolIndex, pageIndex, localIndex);

		SEAssert(pageIndex < m_pages.size(), "Batch index out of bounds");
		SEAssert(localIndex < BatchPoolPage::k_pageSize, "Local batch index out of bounds");

		{
			std::shared_lock<std::shared_mutex> readLock(m_poolMutex);
	
			m_pages[pageIndex]->ReleaseBatch(localIndex, m_currentFrameNum);
		}
	}


	gr::Batch const* BatchPool::GetBatch(PoolIndex poolIndex) noexcept
	{
		uint32_t pageIndex, localIndex;
		PoolIndexToPageLocalIndexes(poolIndex, pageIndex, localIndex);

		SEAssert(pageIndex < m_pages.size(), "Batch index out of bounds");
		SEAssert(localIndex < BatchPoolPage::k_pageSize, "Local batch index out of bounds");

		{
			std::shared_lock<std::shared_mutex> readLock(m_poolMutex);
				
			return &m_pages[pageIndex]->GetBatch(localIndex);
		}
	}
}