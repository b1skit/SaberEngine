#pragma once

#include <unordered_map>
#include <memory>
#include <array>
#include <mutex>

#include "ParameterBlock.h"


namespace re
{
	class ParameterBlockAllocator
	{
	public:
		ParameterBlockAllocator() = default;
		~ParameterBlockAllocator() { Destroy(); }

		void Destroy();

		void UpdateMutableParamBlocks(); // TODO: Deprecate this once we switch to a double buffer
		void EndOfFrame(); // Clears single-frame PBs


	private:
		void RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, size_t numBytes);

	
	private:
		typedef uint64_t Handle; // == NamedObject::UniqueID()

		struct CommitMetadata
		{
			ParameterBlock::PBType m_type;
			size_t m_startIndex;			// Index of 1st byte
			size_t m_numBytes;				// Total number of allocated bytes
		};

		struct SingleBufferedAllocation
		{
			std::vector<uint8_t> m_committed;
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_handleToPtr;
		};

		struct DoubleBufferedAllocation
		{
			std::vector<uint8_t> m_committed; // TODO: Double buffer this
			std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_handleToPtr;
		};


	private:
		
		SingleBufferedAllocation m_immutableAllocations;
		DoubleBufferedAllocation m_mutableAllocations;
		DoubleBufferedAllocation m_singleFrameAllocations;

		std::unordered_map<Handle, CommitMetadata> m_uniqueIDToTypeAndByteIndex;

		std::recursive_mutex m_dataMutex;
		// TODO: Implement a mutex per allocation type + for the m_uniqueIDToTypeAndByteIndex?


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
