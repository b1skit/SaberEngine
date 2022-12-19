#include "ParameterBlockAllocator.h"
#include "DebugConfiguration.h"
#include "ParameterBlock_Platform.h"

using re::ParameterBlock;
using std::shared_ptr;
using std::unordered_map;
using std::shared_ptr;


namespace re
{
	ParameterBlockAllocator::ParameterBlockAllocator()
		: m_allocationPeriodEnded(false)
		, m_permanentPBsHaveBeenBuffered(false)
		, m_readFrameNum(1) // Start at 1 so GetWriteIdx() == 0 for 1st commits
	{
	}
	

	void ParameterBlockAllocator::Destroy()
	{
		std::scoped_lock lock(
			m_uniqueIDToTypeAndByteIndexMutex,
			m_immutableAllocations.m_mutex, 
			m_mutableAllocations.m_mutex,
			m_singleFrameAllocations.m_mutex);

		// Must clear the parameter blocks shared_ptrs before clearing the committed memory
		m_immutableAllocations.m_handleToPtr.clear();
		m_mutableAllocations.m_handleToPtrAndDirty.clear();
		m_singleFrameAllocations.m_handleToPtr.clear();

		// Clear the committed memory
		m_immutableAllocations.m_committed.clear();
		for (size_t i = 0; i < k_numBuffers; i++)
		{
			m_mutableAllocations.m_committed[i].clear();
			m_singleFrameAllocations.m_committed[i].clear();
		}

		// Clear the handle -> commit map
		m_uniqueIDToTypeAndByteIndex.clear();
	}


	void ParameterBlockAllocator::ClosePermanentPBRegistrationPeriod()
	{
		m_allocationPeriodEnded = true;
	}


	void ParameterBlockAllocator::SwapBuffers(uint64_t renderFrameNum)
	{
		m_readFrameNum = renderFrameNum;
	}


	void ParameterBlockAllocator::RegisterAndAllocateParameterBlock(std::shared_ptr<re::ParameterBlock> pb, size_t numBytes)
	{
		SEAssert("Permanent parameter blocks can only be registered at startup, before the 1st render frame", 
			pb->GetType() == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded);

		const ParameterBlock::PBType pbType = pb->GetType();
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			SEAssert("Parameter block is already registered",
				!m_singleFrameAllocations.m_handleToPtr.contains(pb->GetUniqueID()));
			m_singleFrameAllocations.m_handleToPtr[pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);

			SEAssert("Parameter block is already registered", 
				!m_immutableAllocations.m_handleToPtr.contains(pb->GetUniqueID()));
			m_immutableAllocations.m_handleToPtr[pb->GetUniqueID()] = pb;
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			SEAssert("Parameter block is already registered",
				!m_mutableAllocations.m_handleToPtrAndDirty.contains(pb->GetUniqueID()));

			m_mutableAllocations.m_handleToPtrAndDirty[pb->GetUniqueID()] = 
				std::pair<std::shared_ptr<ParameterBlock>, bool>{ pb, true };
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
		SEAssert("Permanent parameter blocks can only be allocated at startup, before the 1st render frame",
			pbType == ParameterBlock::PBType::SingleFrame || !m_allocationPeriodEnded);

		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			SEAssert("A parameter block with this handle has already been added",
				m_uniqueIDToTypeAndByteIndex.find(uniqueID) == m_uniqueIDToTypeAndByteIndex.end());
		}

		// Get the index we'll be inserting the 1st byte of our data to, resize the vector, and initialize it with zeros
		size_t dataIndex;
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			const size_t writeIdx = GetWriteIdx();

			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			dataIndex = m_singleFrameAllocations.m_committed[writeIdx].size();
			
			const size_t resizeAmt = m_singleFrameAllocations.m_committed[writeIdx].size() + numBytes;
			m_singleFrameAllocations.m_committed[writeIdx].resize(resizeAmt, 0);
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);

			dataIndex = m_immutableAllocations.m_committed.size();
			m_immutableAllocations.m_committed.resize(m_immutableAllocations.m_committed.size() + numBytes, 0);
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			SEAssert("Allocations are out of sync",
				m_mutableAllocations.m_committed[0].size() == m_mutableAllocations.m_committed[1].size());

			dataIndex = m_mutableAllocations.m_committed[0].size();

			const size_t resizeAmt = m_mutableAllocations.m_committed[0].size() + numBytes;
			m_mutableAllocations.m_committed[0].resize(resizeAmt, 0);
			m_mutableAllocations.m_committed[1].resize(resizeAmt, 0);
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
			dataIndex = static_cast<size_t>(-1); // Make our insertion index obviously incorrect
		}
		}

		// Update our ID -> data tracking table:
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);
			m_uniqueIDToTypeAndByteIndex.insert({ uniqueID, {pbType, dataIndex, numBytes} });
		}
	}


	void ParameterBlockAllocator::Commit(Handle uniqueID, void const* data)
	{
		size_t startIdx;
		size_t numBytes;
		ParameterBlock::PBType pbType;
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& result = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

			SEAssert("Parameter block with this ID has not been allocated",
				result != m_uniqueIDToTypeAndByteIndex.end());

			SEAssert("Immutable parameter blocks can only be committed at startup",
				!m_allocationPeriodEnded || result->second.m_type != ParameterBlock::PBType::Immutable);

			startIdx = result->second.m_startIndex;
			numBytes = result->second.m_numBytes;
			pbType = result->second.m_type;
		}

		// Copy the data to our pre-allocated region.
		// Note: We still need to lock our mutexes before copying, incase the vectors are resized by another allocation
		void* dest = nullptr;
		const size_t writeIdx = GetWriteIdx();
		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
			dest = &m_singleFrameAllocations.m_committed[writeIdx][startIdx];
			memcpy(dest, data, numBytes);
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			dest = &m_immutableAllocations.m_committed[startIdx];
			memcpy(dest, data, numBytes);
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
			dest = &m_mutableAllocations.m_committed[writeIdx][startIdx];
			memcpy(dest, data, numBytes);
			m_mutableAllocations.m_handleToPtrAndDirty[uniqueID].second = true; // Mark dirty
		}
		break;
		default:
		{
			SEAssertF("Invalid Parameter Block type");
		}
		}


		// If this is the 1st commit, we need to also copy the data to the other side of the mutables double buffer,
		// incase it doesn't get a an update the next frame
		if (!m_allocationPeriodEnded && pbType == ParameterBlock::PBType::Mutable)
		{
			const size_t readIdx = GetReadIdx();

			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);			
			dest = &m_mutableAllocations.m_committed[readIdx][startIdx];
			memcpy(dest, data, numBytes);
		}
	}


	void ParameterBlockAllocator::Get(Handle uniqueID, void*& out_data, size_t& out_numBytes)
	{
		ParameterBlock::PBType pbType;
		size_t startIdx;
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& result = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

			SEAssert("Parameter block with this ID has not been allocated",
				result != m_uniqueIDToTypeAndByteIndex.end());

			out_numBytes = result->second.m_numBytes;

			pbType = result->second.m_type;
			startIdx = result->second.m_startIndex;
		}

		const size_t readIdx = GetReadIdx(); // Get() is called when binding PBs to shaders

		switch (pbType)
		{
		case ParameterBlock::PBType::SingleFrame:
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
			out_data = &m_singleFrameAllocations.m_committed[readIdx][startIdx];
		}
		break;
		case ParameterBlock::PBType::Immutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);
			out_data = &m_immutableAllocations.m_committed[startIdx];
		}
		break;
		case ParameterBlock::PBType::Mutable:
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);
			out_data = &m_mutableAllocations.m_committed[readIdx][startIdx];
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
		ParameterBlock::PBType pbType;
		size_t startIdx;
		size_t numBytes;
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& pb = m_uniqueIDToTypeAndByteIndex.find(uniqueID);

			SEAssert("Cannot deallocate a parameter block that does not exist", pb != m_uniqueIDToTypeAndByteIndex.end());

			pbType = pb->second.m_type;
			startIdx = pb->second.m_startIndex;
			numBytes = pb->second.m_numBytes;
		}

		switch (pbType)
		{
		case ParameterBlock::PBType::Immutable:
		case ParameterBlock::PBType::Mutable:
		{
			// Do nothing: Permanent PBs are held for the lifetime of the program
		}
		break;		
		case ParameterBlock::PBType::SingleFrame:
		{
			// Deallocation uses the read index, as PBs are typically destroyed right before we clear them
			const size_t readIdx = GetReadIdx();

			// We zero out the allocation here. This isn't actually necessary (since we clear all single frame 
			// allocations during EndOfFrame()), but is intended to simplify debugging
			const size_t singleFrameIdx = static_cast<size_t>(ParameterBlock::PBType::SingleFrame);

			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);
			memset(&m_singleFrameAllocations.m_committed[readIdx][startIdx], 0, numBytes);
		}
		break;
		default:
			SEAssertF("Invalid parameter block type");
		}

		// Remove the ID from our map
		{
			std::lock_guard<std::recursive_mutex> lock(m_uniqueIDToTypeAndByteIndexMutex);

			auto const& pb = m_uniqueIDToTypeAndByteIndex.find(uniqueID);
			m_uniqueIDToTypeAndByteIndex.erase(pb);
		}
	}


	void ParameterBlockAllocator::BufferParamBlocks()
	{
		SEAssert("Cannot buffer param blocks until they're all allocated", m_allocationPeriodEnded);

		// Create/buffer Mutable PBs, if they've been modified
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutableAllocations.m_mutex);

			for (auto& pb : m_mutableAllocations.m_handleToPtrAndDirty)
			{
				if (pb.second.second == true)
				{
					platform::ParameterBlock::Update(*pb.second.first.get());
					pb.second.second = false; // Mark clean
				}
			}
		}

		// Create/buffer SingleFrame PBs 
		{
			std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

			for (auto const& pb : m_singleFrameAllocations.m_handleToPtr)
			{
				platform::ParameterBlock::Create(*pb.second.get());
			}
		}

		// Create/buffer Immutable PBs once
		if (!m_permanentPBsHaveBeenBuffered)
		{
			m_permanentPBsHaveBeenBuffered = true;

			std::lock_guard<std::recursive_mutex> lock(m_immutableAllocations.m_mutex);

			for (auto const& pb : m_immutableAllocations.m_handleToPtr)
			{
				platform::ParameterBlock::Create(*pb.second.get());
			}
		}
	}


	void ParameterBlockAllocator::EndOfFrame()
	{
		const size_t readIdx = GetReadIdx();

		std::lock_guard<std::recursive_mutex> lock(m_singleFrameAllocations.m_mutex);

		// This is compiled out in Release
		for (auto const& it : m_singleFrameAllocations.m_handleToPtr)
		{
			SEAssert("Trying to deallocate a single frame parameter block, but there is still a live shared_ptr. Is "
				"something holding onto a single frame parameter block beyond the frame lifetime?", 
				it.second.use_count() == 1);
		}

		m_singleFrameAllocations.m_handleToPtr.clear(); // PB destructors call Deallocate(), so destroy shared_ptrs 1st
		m_singleFrameAllocations.m_committed[readIdx].clear();
	}
}