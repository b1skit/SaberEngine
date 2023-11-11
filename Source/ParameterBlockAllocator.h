// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "ParameterBlock.h"


namespace re
{
	class ParameterBlockAllocator
	{
	public:
		ParameterBlockAllocator();
		ParameterBlockAllocator(ParameterBlockAllocator&&) = default;
		ParameterBlockAllocator& operator=(ParameterBlockAllocator&&) = default;
		~ParameterBlockAllocator();

		void Destroy();
		bool IsValid() const; // Has Destroy() been called?

		void BufferParamBlocks();

		void ClosePermanentPBRegistrationPeriod(); // Called once after all permanent PBs are created

		void SwapBuffers(uint64_t renderFrameNum); // renderFrameNum is always 1 behind the front end thread
		void EndOfFrame(); // Clears single-frame PBs


	private:
		void RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, uint32_t numBytes);

	
	private:
		typedef uint64_t Handle; // == NamedObject::UniqueID()

		static constexpr uint8_t k_numBuffers = 2;
		static constexpr uint32_t k_fixedAllocationByteSize = 16 * 1024 * 1024; // Arbitrary; Increase as necessary

		struct CommitMetadata
		{
			ParameterBlock::PBType m_type;
			uint32_t m_startIndex;	// Index of 1st byte
			uint32_t m_numBytes;		// Total number of allocated bytes
		};

		struct ImmutableAllocation // Single buffered
		{
			std::vector<uint8_t> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_handleToPtr;
			mutable std::recursive_mutex m_mutex;
		} m_immutableAllocations;

		// TODO: Double-buffered allocations should have 2 mutexes (or a read/write mutex)

		struct MutableAllocation // Double buffered
		{
			std::array<std::vector<uint8_t>, k_numBuffers> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_handleToPtr;
			mutable std::recursive_mutex m_mutex;
		} m_mutableAllocations;

		struct SingleFrameAllocation // Double buffered
		{
			// Single frame allocations are stack allocated from a fixed-size, double-buffered array
			std::array<std::array<uint8_t, k_fixedAllocationByteSize>, k_numBuffers> m_committed;
			std::array<std::uint32_t, k_numBuffers> m_baseIdx;
			std::array<std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>>, k_numBuffers> m_handleToPtr;
			mutable std::recursive_mutex m_mutex;
		} m_singleFrameAllocations;

		uint32_t m_maxSingleFrameAllocations; // Debug: Track the high-water mark for the max single-frame PB allocations
		uint32_t m_maxSingleFrameAllocationByteSize;

		std::unordered_map<Handle, CommitMetadata> m_uniqueIDToTypeAndByteIndex;
		mutable std::recursive_mutex m_uniqueIDToTypeAndByteIndexMutex;

		std::array<std::queue<Handle>, k_numBuffers> m_dirtyParameterBlocks;
		std::mutex m_dirtyParameterBlocksMutex;


	private:
		void ClearDeferredDeletions(uint64_t frameNum);
		void AddToDeferredDeletions(uint64_t frameNum, std::shared_ptr<re::ParameterBlock>);
		std::queue<std::pair<uint64_t, std::shared_ptr<re::ParameterBlock>>> m_deferredDeleteQueue;
		std::mutex m_deferredDeleteQueueMutex;

	private:
		uint64_t m_readFrameNum; // Render thread read frame # is always 1 behind the front end thread write frame

		uint64_t GetReadIdx() const { return m_readFrameNum % k_numBuffers; }
		uint64_t GetWriteIdx() const { return (m_readFrameNum + 1 ) % k_numBuffers; }
		

	private:
		bool m_allocationPeriodEnded; // Debugging helper: Used to assert we're not creating PBs after startup
		bool m_permanentPBsHaveBeenBuffered;
		bool m_isValid;

	private: // Interfaces for the ParameterBlock friend class:
		void Allocate(Handle uniqueID, uint32_t numBytes, ParameterBlock::PBType pbType); // Called once at creation
		void Commit(Handle uniqueID, void const* data);	// Update the parameter block data held by the allocator
		
		void GetDataAndSize(Handle uniqueID, void const*& out_data, uint32_t& out_numBytes) const; // Get PB metadata
		uint32_t GetSize(Handle uniqueID) const;

		void Deallocate(Handle uniqueID);


	private:
		ParameterBlockAllocator(ParameterBlockAllocator const&) = delete;
		ParameterBlockAllocator& operator=(ParameterBlockAllocator const&) = delete;

		friend class re::ParameterBlock;
	};
}
