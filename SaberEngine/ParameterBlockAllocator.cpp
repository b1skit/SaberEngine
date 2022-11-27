#include "ParameterBlockAllocator.h"
#include "DebugConfiguration.h"
#include "ParameterBlock_Platform.h"

using re::ParameterBlock;
using std::shared_ptr;
using std::unordered_map;
using std::shared_ptr;


namespace re
{
	void ParameterBlockAllocator::Destroy()
	{
		m_immutablePBs.clear();
		m_mutablePBs.clear();
		m_singleFramePBs.clear();
	}


	void ParameterBlockAllocator::RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, size_t numBytes)
	{
		PBType pbType;
		if (pb->GetLifetime() == re::ParameterBlock::Lifetime::SingleFrame)
		{
			SEAssert("Parameter block is already registered", !m_singleFramePBs.contains(pb->GetUniqueID()));
			m_singleFramePBs[pb->GetUniqueID()] = pb;
			pbType = PBType::SingleFrame;
		}
		else
		{
			switch (pb->GetUpdateType())
			{
			case ParameterBlock::UpdateType::Immutable:
			{
				SEAssert("Parameter block is already registered", !m_immutablePBs.contains(pb->GetUniqueID()));
				m_immutablePBs[pb->GetUniqueID()] = pb;
				pbType = PBType::Immutable;
			}
			break;
			case ParameterBlock::UpdateType::Mutable:
			{
				SEAssert("Parameter block is already registered", !m_mutablePBs.contains(pb->GetUniqueID()));
				m_mutablePBs[pb->GetUniqueID()] = pb;
				pbType = PBType::Mutable;
			}
			break;
			default:
			{
				SEAssertF("Invalid update type");
			}
			}
		}

		// Pre-allocate our PB so it's ready to commit to:
		Allocate(pb->GetUniqueID(), numBytes, pb->GetUpdateType(), pb->GetLifetime());
	}


	void ParameterBlockAllocator::Allocate(
		Handle uniqueID, size_t numBytes, ParameterBlock::UpdateType updateType, ParameterBlock::Lifetime lifetime)
	{
		SEAssert("A parameter block with this handle has already been added",
			m_data.m_uniqueIDToTypeAndByteIndex.find(uniqueID) == m_data.m_uniqueIDToTypeAndByteIndex.end());

		PBType pbType;
		if (lifetime == ParameterBlock::Lifetime::SingleFrame)
		{
			pbType = PBType::SingleFrame;
		}
		else
		{
			switch (updateType)
			{
			case ParameterBlock::UpdateType::Mutable:
			{
				pbType = PBType::Mutable;
			}
			break;
			case ParameterBlock::UpdateType::Immutable:
			{
				pbType = PBType::Immutable;
			}
			break;
			default:
				SEAssertF("Invalid UpdateType");
			}
		}

		// Record the index we'll be inserting the 1st byte of our data to
		const size_t pbTypeIdx = static_cast<size_t>(pbType);
		const size_t dataIndex = m_data.m_committed[pbTypeIdx].size();

		// Resize the vector, and initialize it with zeros
		m_data.m_committed[pbTypeIdx].resize(m_data.m_committed[pbTypeIdx].size() + numBytes, 0);

		// Update our ID -> data tracking table:
		m_data.m_uniqueIDToTypeAndByteIndex.insert({ uniqueID, {pbType, dataIndex, numBytes} });
	}


	void ParameterBlockAllocator::Commit(Handle uniqueID, void const* data)
	{
		auto const& result = m_data.m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Parameter block with this ID has not been allocated", 
			result != m_data.m_uniqueIDToTypeAndByteIndex.end());

		// Copy the data to our pre-allocated region:
		const size_t mapType = static_cast<size_t>(result->second.m_type);
		const size_t startIdx = result->second.m_startIndex;
		const size_t numBytes = result->second.m_numBytes;

		void* const dest = &m_data.m_committed[mapType][startIdx];

		memcpy(dest, data, numBytes);
	}


	void ParameterBlockAllocator::Get(Handle uniqueID, void*& out_data, size_t& out_numBytes)
	{
		auto const& result = m_data.m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Parameter block with this ID has not been allocated",
			result != m_data.m_uniqueIDToTypeAndByteIndex.end());

		const size_t mapType = static_cast<size_t>(result->second.m_type);
		const size_t startIdx = result->second.m_startIndex;
		
		out_data = &m_data.m_committed[mapType][startIdx];
		out_numBytes = result->second.m_numBytes;
	}


	void ParameterBlockAllocator::Deallocate(Handle uniqueID)
	{
		auto const& pb = m_data.m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Cannot deallocate a parameter block that does not exist", 
			pb != m_data.m_uniqueIDToTypeAndByteIndex.end());

		switch (pb->second.m_type)
		{
		case PBType::Immutable:
		{
			// TODO: Repack the parameter block data so it's contiguous again.
		}
		break;
		case PBType::Mutable:
		{
			// TODO: Repack the parameter block data so it's contiguous again.
		}
		break;
		case PBType::SingleFrame:
		{
			break; // We clear all allocations during EndOfFrame() call
		}
		break;
		default:
			SEAssertF("Invalid parameter block type");
		}

		// Remove the ID from our map
		m_data.m_uniqueIDToTypeAndByteIndex.erase(pb);
	}


	void ParameterBlockAllocator::UpdateParamBlocks()
	{
		for (auto const& pb : m_mutablePBs) // Immutable and single-frame PBs are buffered at creation
		{
			if (pb.second->GetDirty())
			{
				platform::ParameterBlock::Update(*pb.second.get());
			}
		}
	}


	void ParameterBlockAllocator::EndOfFrame()
	{
		m_singleFramePBs.clear(); // PB destructors call Deallocate()

		m_data.m_committed[static_cast<size_t>(PBType::SingleFrame)].clear();
	}
}