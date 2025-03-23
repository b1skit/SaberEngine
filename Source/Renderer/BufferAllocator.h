// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IPlatformParams.h"
#include "Buffer.h"


namespace re
{
	class BufferAllocator
	{
	public:
		// Arbitrary. GPU buffer size for stack-allocated single frame buffers
		static constexpr uint32_t k_sharedSingleFrameAllocationByteSize = 64 * 1024 * 1024;
		
		// Reservation size for temporary CPU-side commit buffers
		static constexpr uint32_t k_temporaryReservationBytes = 64 * 1024 * 1024; 

		// No. of permanent mutable buffers we expect to see
		static constexpr uint32_t k_permanentReservationCount = 128; 


	public:
		static std::unique_ptr<re::BufferAllocator> Create();


	public:
		virtual void Initialize(uint64_t currentFrame);

		virtual ~BufferAllocator();

		virtual void Destroy();

		virtual void BufferDataPlatform() = 0; // API-specific data buffering

		bool IsValid() const; // Has Destroy() been called?


	public:
		void CreateBufferPlatformObjects() const; // Trigger platform creation for any new buffers in the dirty list
		void BufferData(uint64_t renderFrameNum);

	private:
		void ResetForNewFrame(uint64_t renderFrameNum);
		void ClearTemporaryStaging();


	protected:
		BufferAllocator();


	public:
		// For single-frame resources, to ensure resources are available throughout their lifetime we allocate one
		// buffer in the upload heap, per each of the maximum number of frames in flight.
		// 
		// Single-frame resources are stack-allocated from these heaps, AND maintained for a fixed lifetime of N 
		// frames. We only write into 1 array of each type at a time, thus only need 1 base index per AllocationPool.
		// 
		// We select the pool with the smallest alignment that will satisfy the Buffer's Usage flags.
		//
		// We maintain the stack base indexes here, and let the API-layer figure out how to interpret/use it.
		//
		enum AllocationPool : uint8_t
		{
			Raw,			// 16B aligned data (E.g. Vertex/index buffers, byte address buffers, etc)
			Constant,		// 256B aligned
			Structured,		// 64KB aligned			

			AllocationPool_Count
		};
		static AllocationPool BufferUsageMaskToAllocationPool(re::Buffer::UsageMask);

	protected:
		uint32_t AdvanceBaseIdx(AllocationPool, uint32_t alignedSize);
		uint8_t GetSingleFrameGPUWriteIndex() const;


	private:
		std::array<std::atomic<uint32_t>, AllocationPool_Count> m_bufferBaseIndexes;
		uint8_t m_singleFrameGPUWriteIdx;

	
	private:
		typedef UniqueID Handle;

		static constexpr uint32_t k_invalidStartIdx = std::numeric_limits<uint32_t>::max();

		struct CommitMetadata
		{
			Buffer::StagingPool m_stagingPool = Buffer::StagingPool::StagingPool_Invalid;
			re::Lifetime m_bufferLifetime;
			uint32_t m_startIndex = k_invalidStartIdx;	// Temporary: Index of 1st byte. Permanent: Commit array index
			uint32_t m_totalBytes = 0;	// Total number of allocated bytes
		};

		struct IAllocation
		{
			virtual ~IAllocation() = default;

			std::unordered_map<Handle, std::shared_ptr<re::Buffer>> m_handleToPtr;

			uint32_t m_totalAllocations = 0;
			uint32_t m_totalAllocationsByteSize = 0; // Total bytes over program lifetime
			uint32_t m_currentAllocationsByteSize = 0;
			uint32_t m_maxAllocations = 0;
			uint32_t m_maxAllocationsByteSize = 0; // High water mark

			mutable std::recursive_mutex m_mutex;
		};

		struct MutableAllocation final : public virtual IAllocation
		{
			std::vector<std::vector<uint8_t>> m_committed;

			struct PartialCommit
			{
				uint32_t m_baseOffset;
				uint32_t m_numBytes;
				uint8_t m_numRemainingUpdates; // Decremented each update
			};
			typedef std::list<PartialCommit> CommitRecord;
			std::unordered_map<Handle, CommitRecord> m_partialCommits;
		};
		MutableAllocation m_mutableAllocations;

		struct TemporaryAllocation final : public virtual IAllocation
		{
			std::vector<uint8_t> m_committed; // Cleared after every frame; Temporaries are written to once
		};
		TemporaryAllocation m_immutableAllocations;
		TemporaryAllocation m_singleFrameAllocations;


		std::unordered_map<Handle, CommitMetadata> m_handleToCommitMetadata;
		mutable std::recursive_mutex m_handleToCommitMetadataMutex;

		std::unordered_set<std::shared_ptr<re::Buffer>> m_dirtyBuffers;
		mutable std::mutex m_dirtyBuffersMutex;


	protected:
		// Data required to perform any API-specific buffering steps
		struct PlatformCommitMetadata
		{
			re::Buffer const* m_buffer;
			uint32_t m_baseOffset;
			uint32_t m_numBytes;
		};
		std::vector<PlatformCommitMetadata> m_dirtyBuffersForPlatformUpdate;
		std::mutex m_dirtyBuffersForPlatformUpdateMutex;


	private:
		void ClearDeferredDeletions(uint64_t frameNum);
		void AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::Buffer> const&);
		std::queue<std::pair<uint64_t, std::shared_ptr<re::Buffer>>> m_deferredDeleteQueue;
		std::mutex m_deferredDeleteQueueMutex;


	protected:
		uint8_t m_numFramesInFlight;


	private:
		uint64_t m_currentFrameNum; // Render thread read frame # is always 1 behind the front end thread frame
		
	private:
		bool m_isValid;


	protected: // Interfaces for the Buffer friend class:
		friend class re::Buffer;
		void Register(std::shared_ptr<re::Buffer> const&, uint32_t numBytes);


	private:		
		uint32_t Allocate(Handle uniqueID, uint32_t numBytes, Buffer::StagingPool, re::Lifetime bufferLifetime); // Returns the start index

	protected:
		void Stage(Handle uniqueID, void const* data);	// Update the buffer data
		void StageMutable(Handle uniqueID, void const* data, uint32_t numBytes, uint32_t dstBaseByteOffset);
		
		void GetData(Handle uniqueID, void const** out_data) const;

		void Deallocate(Handle uniqueID);


	private:
		BufferAllocator(BufferAllocator const&) = delete;
		BufferAllocator(BufferAllocator&&) noexcept = delete;
		BufferAllocator& operator=(BufferAllocator const&) = delete;
		BufferAllocator& operator=(BufferAllocator&&) noexcept = delete;
	};


	inline uint8_t BufferAllocator::GetSingleFrameGPUWriteIndex() const
	{
		return m_singleFrameGPUWriteIdx;
	}


	inline BufferAllocator::AllocationPool BufferAllocator::BufferUsageMaskToAllocationPool(re::Buffer::UsageMask mask)
	{
		SEAssert(mask != re::Buffer::Usage::Invalid, "Invalid usage mask");

		if (re::Buffer::HasUsageBit(re::Buffer::Structured, mask))
		{
			return AllocationPool::Structured;
		}
		else if (re::Buffer::HasUsageBit(re::Buffer::Constant, mask))
		{
			return AllocationPool::Constant;
		}

		SEAssert(re::Buffer::HasUsageBit(re::Buffer::Raw, mask), "Unexpected usage mask");

		return AllocationPool::Raw;
	}


	inline bool BufferAllocator::IsValid() const
	{
		return m_isValid;
	}
}
