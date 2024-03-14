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
		
		static constexpr uint32_t k_singleFrameReservationBytes = 64 * 1024 * 1024; // Reservation size for single-frame CPU-side commit buffers
		static constexpr uint32_t k_permanentReservationCount = 64; // No. of buffers permanent we expect to see


	public:
		struct PlatformParams : public re::IPlatformParams
		{
		public:
			PlatformParams();
			virtual ~PlatformParams() = 0;

			void BeginFrame();
			uint8_t GetWriteIndex() const;
			uint32_t AdvanceBaseIdx(re::Buffer::DataType, uint32_t alignedSize);
			
			const uint8_t m_numBuffers;


		protected:
			// For single-frame resources, to ensure resources are available throughout their lifetime we allocate one
			// buffer in the upload heap, per each of the maximum number of frames in flight.
			// 
			// Single-frame resources are stack-allocated from these heaps, AND maintained for a fixed lifetime of N 
			// frames. We only write into 1 array of each type at a time, thus only need 1 base index per DataType.
			//
			// We maintain the stack base indexes here, and let the API-layer figure out how to interpret/use it.
			//
			std::array<std::atomic<uint32_t>, re::Buffer::DataType::DataType_Count> m_bufferBaseIndexes;

		private:
			uint8_t m_writeIdx;
		};


	public:
		BufferAllocator();
		void Create(uint64_t currentFrame);

		~BufferAllocator();
		void Destroy();

		bool IsValid() const; // Has Destroy() been called?

		void BufferData();

		void BeginFrame(uint64_t renderFrameNum);
		void EndFrame(); // Clears single-frame buffers

		BufferAllocator::PlatformParams* GetPlatformParams() const;
		void SetPlatformParams(std::unique_ptr<BufferAllocator::PlatformParams> params);


	private:
		void RegisterAndAllocateBuffer(std::shared_ptr<re::Buffer>, uint32_t numBytes);

	
	private:
		typedef uint64_t Handle; // == NamedObject::UniqueID()

		struct CommitMetadata
		{
			Buffer::Type m_type;
			uint32_t m_startIndex;	// Singleframe: Index of 1st byte. Permanent: Commit array index
			uint32_t m_numBytes;	// Total number of allocated bytes
		};

		struct MutableAllocation
		{
			std::vector<std::vector<uint8_t>> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::Buffer>> m_handleToPtr;

			struct PartialCommit
			{
				uint32_t m_baseOffset;
				uint32_t m_numBytes;
				uint8_t m_numRemainingUpdates; // Decremented each update
			};
			typedef std::list<PartialCommit> CommitRecord;
			std::unordered_map<Handle, CommitRecord> m_partialCommits;
			
			mutable std::recursive_mutex m_mutex;
		};
		MutableAllocation m_mutableAllocations;


		struct ImmutableAllocation
		{
			std::vector<std::vector<uint8_t>> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::Buffer>> m_handleToPtr;
			mutable std::recursive_mutex m_mutex;
		};
		ImmutableAllocation m_immutableAllocations;

		struct SingleFrameAllocation
		{
			std::vector<uint8_t> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::Buffer>> m_handleToPtr;
			mutable std::recursive_mutex m_mutex;
		};
		SingleFrameAllocation m_singleFrameAllocations;

		std::unordered_map<Handle, CommitMetadata> m_handleToTypeAndByteIndex;
		mutable std::recursive_mutex m_handleToTypeAndByteIndexMutex;

		std::unordered_set<Handle> m_dirtyBuffers;
		std::mutex m_dirtyBuffersMutex;

		std::unique_ptr<PlatformParams> m_platformParams;


	private:
		void ClearDeferredDeletions(uint64_t frameNum);
		void AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::Buffer>);
		uint8_t m_numFramesInFlight;
		std::queue<std::pair<uint64_t, std::shared_ptr<re::Buffer>>> m_deferredDeleteQueue;
		std::mutex m_deferredDeleteQueueMutex;


	private:
		uint64_t m_currentFrameNum; // Render thread read frame # is always 1 behind the front end thread frame
		
	private:
		bool m_isValid;

		// Debug: Track the high-water mark for the max single-frame buffer allocations
		uint32_t m_maxSingleFrameAllocations;
		uint32_t m_maxSingleFrameAllocationByteSize;


	protected: // Interfaces for the Buffer friend class:
		friend class re::Buffer;

		void Allocate(Handle uniqueID, uint32_t numBytes, Buffer::Type bufferType); // Called once at creation
		void Commit(Handle uniqueID, void const* data);	// Update the buffer data
		void Commit(Handle uniqueID, void const* data, uint32_t numBytes, uint32_t dstBaseByteOffset);
		
		void GetDataAndSize(Handle uniqueID, void const*& out_data, uint32_t& out_numBytes) const; // Get buffer metadata
		uint32_t GetSize(Handle uniqueID) const;

		void Deallocate(Handle uniqueID);


	private:
		BufferAllocator(BufferAllocator const&) = delete;
		BufferAllocator(BufferAllocator&&) = delete;
		BufferAllocator& operator=(BufferAllocator const&) = delete;
		BufferAllocator& operator=(BufferAllocator&&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline BufferAllocator::PlatformParams::~PlatformParams() {};
}
