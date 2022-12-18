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

		// Must clear the parameter blocks shared_ptrs before clearing the committed memory
		m_immutableAllocations.m_handleToPtr.clear();
		m_mutableAllocations.m_handleToPtr.clear();
		m_singleFrameAllocations.m_handleToPtr.clear();

		// Clear the committed memory
		m_immutableAllocations.m_committed.clear();
		m_mutableAllocations.m_committed.clear();
		m_singleFrameAllocations.m_committed.clear();

		// Clear the handle -> commit map
		m_uniqueIDToTypeAndByteIndex.clear();
	}


	void ParameterBlockAllocator::RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, size_t numBytes)
	{
		std::unique_lock<std::recursive_mutex> writeLock(m_dataMutex);

		const ParameterBlock::PBType pbType = pb->GetType();
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			SEAssert("Parameter block is already registered",
				!m_singleFrameAllocations.m_handleToPtr.contains(pb->GetUniqueID()));
			m_singleFrameAllocations.m_handleToPtr[pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			SEAssert("Parameter block is already registered", 
				!m_immutableAllocations.m_handleToPtr.contains(pb->GetUniqueID()));
			m_immutableAllocations.m_handleToPtr[pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			SEAssert("Parameter block is already registered",
				!m_mutableAllocations.m_handleToPtr.contains(pb->GetUniqueID()));
			m_mutableAllocations.m_handleToPtr[pb->GetUniqueID()] = pb;
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
			m_uniqueIDToTypeAndByteIndex.find(uniqueID) == m_uniqueIDToTypeAndByteIndex.end());

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		size_t dataIndex;
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			dataIndex = m_singleFrameAllocations.m_committed.size();
			m_singleFrameAllocations.m_committed.resize(m_singleFrameAllocations.m_committed.size() + numBytes, 0);
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			dataIndex = m_immutableAllocations.m_committed.size();
			m_immutableAllocations.m_committed.resize(m_immutableAllocations.m_committed.size() + numBytes, 0);
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			dataIndex = m_mutableAllocations.m_committed.size();
			m_mutableAllocations.m_committed.resize(m_mutableAllocations.m_committed.size() + numBytes, 0);
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
			dataIndex = static_cast<size_t>(-1); // Make our insertion index obviously incorrect
		}
		}

		// Update our ID -> data tracking table:
		m_uniqueIDToTypeAndByteIndex.insert({ uniqueID, {pbType, dataIndex, numBytes} });
	}


	void ParameterBlockAllocator::Commit(Handle uniqueID, void const* data)
	{
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

		auto const& result = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Parameter block with this ID has not been allocated", 
			result != m_uniqueIDToTypeAndByteIndex.end());

		// Copy the data to our pre-allocated region:
		const size_t startIdx = result->second.m_startIndex;
		const size_t numBytes = result->second.m_numBytes;
		const ParameterBlock::PBType pbType = result->second.m_type;

		void* dest = nullptr;

		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			dest = &m_singleFrameAllocations.m_committed[startIdx];
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			dest = &m_immutableAllocations.m_committed[startIdx];
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			dest = &m_mutableAllocations.m_committed[startIdx];
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
		}
		}

		memcpy(dest, data, numBytes);
	}


	void ParameterBlockAllocator::Get(Handle uniqueID, void*& out_data, size_t& out_numBytes)
	{
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

		auto const& result = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Parameter block with this ID has not been allocated",
			result != m_uniqueIDToTypeAndByteIndex.end());

		out_numBytes = result->second.m_numBytes;

		const ParameterBlock::PBType pbType = result->second.m_type;
		const size_t startIdx = result->second.m_startIndex;
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			out_data = &m_singleFrameAllocations.m_committed[startIdx];
			
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			out_data = &m_immutableAllocations.m_committed[startIdx];
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			out_data = &m_mutableAllocations.m_committed[startIdx];
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
		}
		}
	}


	void ParameterBlockAllocator::Deallocate(Handle uniqueID)
	{
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

		auto const& pb = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

		SEAssert("Cannot deallocate a parameter block that does not exist", pb != m_uniqueIDToTypeAndByteIndex.end());

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
			memset(&m_singleFrameAllocations.m_committed[pb->second.m_startIndex], 0, pb->second.m_numBytes);
		}
		break;
		default:
			SEAssertF("Invalid parameter block type");
		}

		// Remove the ID from our map
		m_uniqueIDToTypeAndByteIndex.erase(pb);
	}


	void ParameterBlockAllocator::UpdateMutableParamBlocks()
	{
		std::unique_lock<std::recursive_mutex> lock(m_dataMutex);

		for (auto const& pb : m_mutableAllocations.m_handleToPtr) // Immutable & single-frame PBs are buffered at creation
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

		// This is compiled out in Release
		for (auto const& it : m_singleFrameAllocations.m_handleToPtr)
		{
			SEAssert("Trying to deallocate a single frame parameter block, but there is still a live shared_ptr. Is "
				"something holding onto a single frame parameter block beyond the frame lifetime?", 
				it.second.use_count() == 1);
		}

		m_singleFrameAllocations.m_handleToPtr.clear(); // PB destructors call Deallocate(), so destroy shared_ptrs 1st
		m_singleFrameAllocations.m_committed.clear();
	}
}