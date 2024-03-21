// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "Buffer.h"


namespace re
{
	class BufferAllocator
	{
	public:
		static constexpr uint32_t k_fixedAllocationByteSize = 64 * 1024 * 1024; // Arbitrary. Fixed GPU buffer allocation size
		
		static constexpr uint32_t k_temporaryReservationBytes = 64 * 1024 * 1024; // Reservation size for temporary CPU-side commit buffers
		static constexpr uint32_t k_permanentReservationCount = 128; // No. of permanent mutable buffers we expect to see


	public:
		static std::unique_ptr<re::BufferAllocator> Create();


	public:
		virtual void Initialize(uint64_t currentFrame);

		virtual ~BufferAllocator();

		virtual void Destroy();

		virtual void BufferDataPlatform() = 0; // API-specific data buffering


	public:
		void BufferData();

		bool IsValid() const; // Has Destroy() been called?

		void BeginFrame(uint64_t renderFrameNum);
		void EndFrame(); // Clears temporary buffers


	private:
		void RegisterAndAllocateBuffer(std::shared_ptr<re::Buffer>, uint32_t numBytes);


	protected:
		BufferAllocator();


	protected:
		// For single-frame resources, to ensure resources are available throughout their lifetime we allocate one
		// buffer in the upload heap, per each of the maximum number of frames in flight.
		// 
		// Single-frame resources are stack-allocated from these heaps, AND maintained for a fixed lifetime of N 
		// frames. We only write into 1 array of each type at a time, thus only need 1 base index per DataType.
		//
		// We maintain the stack base indexes here, and let the API-layer figure out how to interpret/use it.
		//
		uint32_t AdvanceBaseIdx(re::Buffer::DataType, uint32_t alignedSize);
		uint8_t GetWriteIndex() const;


	private:
		std::array<std::atomic<uint32_t>, re::Buffer::DataType::DataType_Count> m_bufferBaseIndexes;
		uint8_t m_writeIdx;

	
	private:
		typedef uint64_t Handle; // == NamedObject::UniqueID()

		struct CommitMetadata
		{
			Buffer::Type m_type;
			uint32_t m_startIndex;	// Singleframe: Index of 1st byte. Permanent: Commit array index
			uint32_t m_numBytes;	// Total number of allocated bytes
		};

		struct IAllocation
		{
			virtual ~IAllocation() = 0;

			std::unordered_map<Handle, std::shared_ptr<re::Buffer>> m_handleToPtr;

			uint32_t m_totalAllocations;
			uint32_t m_totalAllocationsByteSize; // Total bytes over program lifetime
			uint32_t m_currentAllocationsByteSize;
			uint32_t m_maxAllocations;
			uint32_t m_maxAllocationsByteSize; // High water mark

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


		std::unordered_map<Handle, CommitMetadata> m_handleToTypeAndByteIndex;
		mutable std::recursive_mutex m_handleToTypeAndByteIndexMutex;

		std::unordered_set<Handle> m_dirtyBuffers;
		std::mutex m_dirtyBuffersMutex;


	protected:
		// Data required to perform any API-specific buffering steps
		struct PlatformCommitMetadata
		{
			re::Buffer const* m_buffer;
		};
		std::vector<PlatformCommitMetadata> m_dirtyBuffersForPlatformUpdate;
		std::mutex m_dirtyBuffersForPlatformUpdateMutex;


	private:
		void ClearDeferredDeletions(uint64_t frameNum);
		void AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::Buffer>);
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

		void Allocate(Handle uniqueID, uint32_t numBytes, Buffer::Type bufferType); // Called once at creation
		void Commit(Handle uniqueID, void const* data);	// Update the buffer data
		void Commit(Handle uniqueID, void const* data, uint32_t numBytes, uint32_t dstBaseByteOffset);
		
		void GetData(Handle uniqueID, void const** out_data) const;

		void Deallocate(Handle uniqueID);


	private:
		BufferAllocator(BufferAllocator const&) = delete;
		BufferAllocator(BufferAllocator&&) = delete;
		BufferAllocator& operator=(BufferAllocator const&) = delete;
		BufferAllocator& operator=(BufferAllocator&&) = delete;
	};


	inline uint8_t BufferAllocator::GetWriteIndex() const
	{
		return m_writeIdx;
	}


	inline bool BufferAllocator::IsValid() const
	{
		return m_isValid;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline BufferAllocator::IAllocation::~IAllocation() {};
}
