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

		void UpdateParamBlocks(); // TODO: Deprecate this once we switch to a double buffer
		void EndOfFrame(); // Clears single-frame PBs


	private:
		void RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, size_t numBytes);

		enum class PBType
		{
			Immutable,
			Mutable,
			SingleFrame,
			PBType_Count
		};
		typedef uint64_t Handle; // == NamedObject::UniqueID()

		// We hold a reference to all parameter blocks so we can pump update on mutable PBs; We don't really need
		// to hold them for immutable or single frame PBs, but no harm for now
		std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_immutablePBs;
		std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_mutablePBs;
		std::unordered_map<Handle, std::shared_ptr<re::ParameterBlock>> m_singleFramePBs;
		
		struct
		{
			struct CommitMetadata
			{
				PBType m_type;
				size_t m_startIndex; // Index of 1st byte
				size_t m_numBytes;	// Total number of allocated bytes
			};

			std::array<std::vector<uint8_t>, static_cast<size_t>(PBType::PBType_Count)> m_committed;
			std::unordered_map<Handle, CommitMetadata> m_uniqueIDToTypeAndByteIndex;
		} m_data;
		std::recursive_mutex m_dataMutex;


	private:
		// Interfaces for the ParameterBlock friend class:
		void Allocate(
			Handle uniqueID, size_t numBytes, ParameterBlock::UpdateType updateType, ParameterBlock::Lifetime lifetime); // Called once at creation
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
