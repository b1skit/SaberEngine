// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"

#include "Core/Util/HashKey.h"


namespace gr
{
	class BatchHandle;
	class ComputeBatchBuilder;
	template<typename BuilderImpl>
	class IBatchBuilder;
	class RasterBatchBuilder;
	class RayTraceBatchBuilder;
}

namespace re
{
	class Batch;
}

namespace gr
{
	using PoolIndex = uint32_t;


	class BatchPoolPage
	{
	public:
		static constexpr size_t k_pageSize = 1024;


	public:
		BatchPoolPage(uint32_t baseIndex, uint8_t numFramesInFlight) noexcept;

		BatchPoolPage(BatchPoolPage&&) noexcept = default;
		BatchPoolPage& operator=(BatchPoolPage&&) noexcept = default;

		void Update(uint64_t currentFrameNum, std::unordered_map<util::HashKey, PoolIndex>&) noexcept;

		void Destroy(std::unordered_map<util::HashKey, PoolIndex>& batchHashToIndexMap);


	public:
		bool AddBatch(re::Batch&& batch, PoolIndex& outIndex) noexcept;

		void AddBatchRef(uint32_t pageIndex) noexcept; // Increments the ref count for the batch at pageIndex

		// Decrements the ref count, removes the batch if it reaches 0
		void ReleaseBatch(uint32_t pageIndex, uint64_t currentFrameNum) noexcept; 
		
		re::Batch const& GetBatch(uint32_t pageIndex) noexcept;		


	private:
		void ProcessDeferredDeletes(
			uint64_t currentFrameNum, std::unordered_map<util::HashKey, PoolIndex>& batchHashToIndexMap);


	private:
		static constexpr size_t k_cacheAlignment = 64;
		struct alignas(k_cacheAlignment) AlignedRefCount
		{
			AlignedRefCount() noexcept : m_value(0), _padding{0} {}

			std::atomic<uint32_t> m_value;
			uint8_t _padding[k_cacheAlignment - sizeof(std::atomic<uint32_t>)]; // Guarantee correct alignment
		};
		SEStaticAssert(sizeof(AlignedRefCount) == k_cacheAlignment, "Struct is not cache aligned");

		std::array<re::Batch, k_pageSize> m_batches;
		std::array<AlignedRefCount, k_pageSize> m_batchRefCounts; // Reference counts for each batch

		std::vector<uint32_t> m_freeIndexes; // Free indices in the page: Relative to m_batches, not the overall pool

		// Batches that have had a ref count of 0 at some point
		std::queue<std::pair<uint32_t, uint64_t>> m_deferredDeletes;

		uint32_t m_baseIndex; // Base index of this page in the overall pool

		const uint8_t m_numFramesInFlight;

		std::mutex m_pageMutex;


	private:
		BatchPoolPage(BatchPoolPage const&) = delete;
		BatchPoolPage& operator=(BatchPoolPage const&) = delete;
	};


	// ---


	class BatchPool
	{
	public:
		BatchPool(uint8_t numFramesInFlight);

		BatchPool(BatchPool&&) = default;
		BatchPool& operator=(BatchPool&&) = default;
		~BatchPool() = default;

		void Destroy();


	public:
		void Update(uint64_t currentFrameNum);


	private:
		friend class gr::ComputeBatchBuilder;
		template<typename BuilderImpl>
		friend class gr::IBatchBuilder;
		friend class gr::RasterBatchBuilder;
		friend class gr::RayTraceBatchBuilder;

		gr::BatchHandle AddBatch(re::Batch&& batch, gr::RenderDataID) noexcept;

		void AddBatchRef(PoolIndex) noexcept; // Increments the ref count for the batch

		void ReleaseBatch(PoolIndex) noexcept;


	private:
		friend class BatchHandle;
		re::Batch const* GetBatch(PoolIndex) noexcept; // Returns nullptr if the index is invalid
		

	private:
		std::vector<std::unique_ptr<BatchPoolPage>> m_pages;

		std::unordered_map<util::HashKey, PoolIndex> m_batchHashToIndexMap;

		std::shared_mutex m_poolMutex;

		uint64_t m_currentFrameNum;
		uint8_t m_numFramesInFlight;


	private:
		BatchPool(BatchPool const&) = delete;
		BatchPool& operator=(BatchPool const&) = delete;
	};
}