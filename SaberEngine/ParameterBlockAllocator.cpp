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
		std::unique_lock<std::recursive_mutex> writeLock(m_dataMutex);

		m_immutablePBs.clear();
		m_mutablePBs.clear();
		m_singleFramePBs.clear();

		for (size_t i = 0; i < static_cast<size_t>(ParameterBlock::PBType::PBType_Count); i++)
		{
			m_data.m_committed[i].clear();
		}
		m_data.m_uniqueIDToTypeAndByteIndex.clear();
	}


	void ParameterBlockAllocator::RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, size_t numBytes)
	{
		std::unique_lock<std::recursive_mutex> writeLock(m_dataMutex);

		const ParameterBlock::PBType pbType = pb->GetType();
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			SEAssert("Parameter block is already registered", !m_singleFramePBs.contains(pb->GetUniqueID()));
			m_singleFramePBs[pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			SEAssert("Parameter block is already registered", !m_immutablePBs.contains(pb->GetUniqueID()));
			m_immutablePBs[pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			SEAssert("Parameter block is already registered", !m_mutablePBs.contains(pb->GetUniqueID()));
			m_mutablePBs[pb->GetUniqueID()] = pb;
		}
		break;
		default:
		{
			SEAssertF("Invalid update type");
		}
		}

		// Pre-allocate our PB so it's ready to commit to:
		Allocate(pb->GetUniqueID(), numBytes, pbType);
	}


	void ParameterBlockAllocator::Allocate(Handle uniqueID, size_t numBytes, ParameterBlock::PBType pbType)
	{
		std::unique_lock<std::recursive_mutex> writeLock(m_dataMutex);

		SEAssert("A parameter block with this handle has already been added",
			m_data.m_uniqueIDToTypeAndByteIndex.find(uniqueID) == m_data.m_uniqueIDToTypeAndByteIndex.end());

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
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

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
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

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
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

		auto const& pb = m_data.m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Cannot deallocate a parameter block that does not exist", 
			pb != m_data.m_uniqueIDToTypeAndByteIndex.end());

		switch (pb->second.m_type)
		{
		case ParameterBlock::PBType::Immutable:
		case ParameterBlock::PBType::Mutable:
		{
			// Do nothing: Permanent PBs are held for the lifetime of the program
		}
		break;		
		case ParameterBlock::PBType::SingleFrame:
		{
			// We zero out the allocation here. This isn't actually necessary (since we clear all single frame 
			// allocations during EndOfFrame()), but is intended to simplify debugging
			const size_t singleFrameIdx = static_cast<size_t>(ParameterBlock::PBType::SingleFrame);
			memset(&m_data.m_committed[singleFrameIdx][pb->second.m_startIndex], 0, pb->second.m_numBytes);
		}
		break;
		default:
			SEAssertF("Invalid parameter block type");
		}

		// Remove the ID from our map
		m_data.m_uniqueIDToTypeAndByteIndex.erase(pb);
	}


	void ParameterBlockAllocator::UpdateMutableParamBlocks()
	{
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

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
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

		m_singleFramePBs.clear(); // PB destructors call Deallocate()

		m_data.m_committed[static_cast<size_t>(ParameterBlock::PBType::SingleFrame)].clear();
	}
}