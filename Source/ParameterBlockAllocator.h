// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "ParameterBlock.h"


namespace re
{
	class ParameterBlockAllocator
	{
	public:
		static constexpr uint32_t k_fixedAllocationByteSize = 32 * 1024 * 1024; // Arbitrary. Used to allocate API buffers
		static constexpr uint32_t k_systemMemoryReservationSize = 32 * 1024 * 1024; // CPU-side initial commit memory reservation


	public:
		struct PlatformParams : public re::IPlatformParams
		{
		public:
			PlatformParams();
			virtual ~PlatformParams() = 0;

			void BeginFrame();
			uint8_t GetWriteIndex() const;
			uint32_t AdvanceBaseIdx(re::ParameterBlock::PBDataType, uint32_t alignedSize);
			
			const uint8_t m_numBuffers;


		protected:
			// For single-frame resources, to ensure resources are available throughout their lifetime we allocate one
			// buffer in the upload heap, per each of the maximum number of frames in flight.
			// 
			// Single-frame resources are stack-allocated from these heaps, AND maintained for a fixed lifetime of N 
			// frames. We only write into 1 array of each type at a time, thus only need 1 base index per PBDataType.
			//
			// We maintain the stack base indexes here, and let the API-layer figure out how to interpret/use it.
			//
			std::array<std::atomic<uint32_t>, re::ParameterBlock::PBDataType::PBDataType_Count> m_bufferBaseIndexes;

		private:
			uint8_t m_writeIdx;
		};


	public:
		ParameterBlockAllocator();
		void Create();

		~ParameterBlockAllocator();
		void Destroy();

		bool IsValid() const; // Has Destroy() been called?

		void BufferParamBlocks();

		void ClosePermanentPBRegistrationPeriod(); // Called once after all permanent PBs are created

		void BeginFrame(uint64_t renderFrameNum);
		void EndFrame(); // Clears single-frame PBs

		ParameterBlockAllocator::PlatformParams* GetPlatformParams() const;
		void SetPlatformParams(std::unique_ptr<ParameterBlockAllocator::PlatformParams> params);


	private:
		void RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, uint32_t numBytes);

	
	private:
		typedef uint64_t Handle; // == NamedObject::UniqueID()

		struct CommitMetadata
		{
			ParameterBlock::PBType m_type;
			uint32_t m_startIndex;	// Index of 1st byte
			uint32_t m_numBytes;	// Total number of allocated bytes
		};

		struct Allocation
		{
			std::vector<uint8_t> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_handleToPtr;
			mutable std::recursive_mutex m_mutex;
		};
		std::array<Allocation, re::ParameterBlock::PBType::PBType_Count> m_allocations;

		std::unordered_map<Handle, CommitMetadata> m_handleToTypeAndByteIndex;
		mutable std::recursive_mutex m_handleToTypeAndByteIndexMutex;

		std::queue<Handle> m_dirtyParameterBlocks;
		std::mutex m_dirtyParameterBlocksMutex;

		std::unique_ptr<PlatformParams> m_platformParams;

		uint32_t m_maxSingleFrameAllocations; // Debug: Track the high-water mark for the max single-frame PB allocations
		uint32_t m_maxSingleFrameAllocationByteSize;


	private:
		void ClearDeferredDeletions(uint64_t frameNum);
		void AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::ParameterBlock>);
		std::queue<std::pair<uint64_t, std::shared_ptr<re::ParameterBlock>>> m_deferredDeleteQueue;
		std::mutex m_deferredDeleteQueueMutex;

	private:
		uint64_t m_currentFrameNum; // Render thread read frame # is always 1 behind the front end thread frame
		

	private:
		bool m_allocationPeriodEnded; // Debugging helper: Used to assert we're not creating PBs after startup
		bool m_permanentPBsHaveBeenBuffered;
		bool m_isValid;

	private: // Interfaces for the ParameterBlock friend class:
		friend class re::ParameterBlock;

		void Allocate(Handle uniqueID, uint32_t numBytes, ParameterBlock::PBType pbType); // Called once at creation
		void Commit(Handle uniqueID, void const* data);	// Update the parameter block data held by the allocator
		
		void GetDataAndSize(Handle uniqueID, void const*& out_data, uint32_t& out_numBytes) const; // Get PB metadata
		uint32_t GetSize(Handle uniqueID) const;

		void Deallocate(Handle uniqueID);


	private:
		ParameterBlockAllocator(ParameterBlockAllocator const&) = delete;
		ParameterBlockAllocator(ParameterBlockAllocator&&) = delete;
		ParameterBlockAllocator& operator=(ParameterBlockAllocator const&) = delete;
		ParameterBlockAllocator& operator=(ParameterBlockAllocator&&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline ParameterBlockAllocator::PlatformParams::~PlatformParams() {};
}
