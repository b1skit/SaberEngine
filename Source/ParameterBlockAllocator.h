// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "ParameterBlock.h"


namespace re
{
	class ParameterBlockAllocator
	{
	public:
		ParameterBlockAllocator();
		~ParameterBlockAllocator() { Destroy(); }

		void Destroy();

		void BufferParamBlocks();

		void ClosePermanentPBRegistrationPeriod(); // Called once after all permanent PBs are created

		void SwapBuffers(uint64_t renderFrameNum);
		void EndOfFrame(); // Clears single-frame PBs


	private:
		void RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, size_t numBytes);

	
	private:
		typedef uint64_t Handle; // == NamedObject::UniqueID()

		struct CommitMetadata
		{
			ParameterBlock::PBType m_type;
			size_t m_startIndex;	// Index of 1st byte
			size_t m_numBytes;		// Total number of allocated bytes
		};

		struct ImmutableAllocation
		{
			std::vector<uint8_t> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_handleToPtr;
			std::recursive_mutex m_mutex;
		} m_immutableAllocations;

		static constexpr size_t k_numBuffers = 2;

		struct MutableAllocation
		{
			std::array<std::vector<uint8_t>, k_numBuffers> m_committed;
			std::unordered_map<Handle, std::pair<std::shared_ptr<re::ParameterBlock>, bool>> m_handleToPtrAndDirty;
			std::recursive_mutex m_mutex;
		} m_mutableAllocations;

		struct SingleFrameAllocation
		{
			std::array<std::vector<uint8_t>, k_numBuffers> m_committed;
			std::array< std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>>, k_numBuffers> m_handleToPtr;
			std::recursive_mutex m_mutex;
		} m_singleFrameAllocations;

		std::unordered_map<Handle, CommitMetadata> m_uniqueIDToTypeAndByteIndex;
		std::recursive_mutex m_uniqueIDToTypeAndByteIndexMutex;


	private:
		uint64_t m_readFrameNum; // Read frame # is always 1 behind the write frame
		uint64_t GetReadIdx() const { return m_readFrameNum % 2; }
		uint64_t GetWriteIdx() const { return (m_readFrameNum + 1 ) % 2; }
		

	private:
		bool m_allocationPeriodEnded; // Debugging helper: Used to assert we're not creating PBs after startup
		bool m_permanentPBsHaveBeenBuffered;

	private:
		// Interfaces for the ParameterBlock friend class:
		void Allocate(Handle uniqueID, size_t numBytes, ParameterBlock::PBType pbType); // Called once at creation
		void Commit(Handle uniqueID, void const* data);	// Update the parameter block data held by the allocator
		void Get(Handle uniqueID, void*& out_data, size_t& out_numBytes); // Get the parameter block data
		void Deallocate(Handle uniqueID);


	private:
		ParameterBlockAllocator(ParameterBlockAllocator const&) = delete;
		ParameterBlockAllocator(ParameterBlockAllocator&&) = delete;
		ParameterBlockAllocator& operator=(ParameterBlockAllocator const&) = delete;

		friend class re::ParameterBlock;
	};
}
