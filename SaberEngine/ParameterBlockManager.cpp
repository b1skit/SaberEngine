#include "ParameterBlockManager.h"
#include "DebugConfiguration.h"
#include "ParameterBlock_Platform.h"

using re::ParameterBlock;
using std::shared_ptr;
using std::unordered_map;
using std::shared_ptr;


namespace re
{
	uint64_t ParameterBlockManager::RegisterParameterBlock(std::shared_ptr<re::ParameterBlock> pb)
	{
		MapType mapType;
		if (pb->GetLifetime() == re::ParameterBlock::Lifetime::SingleFrame)
		{
			SEAssert("Parameter block is already registered", !m_singleFramePBs.contains(pb->GetUniqueID()));
			m_singleFramePBs[pb->GetUniqueID()] = pb;
			mapType = MapType::SingleFrame;
		}
		else
		{
			switch (pb->GetUpdateType())
			{
			case ParameterBlock::UpdateType::Immutable:
			{
				SEAssert("Parameter block is already registered", !m_immutablePBs.contains(pb->GetUniqueID()));
				m_immutablePBs[pb->GetUniqueID()] = pb;
				mapType = MapType::Immutable;
			}
			break;
			case ParameterBlock::UpdateType::Mutable:
			{
				SEAssert("Parameter block is already registered", !m_mutablePBs.contains(pb->GetUniqueID()));
				m_mutablePBs[pb->GetUniqueID()] = pb;
				mapType = MapType::Mutable;
			}
			break;
			default:
			{
				SEAssertF("Invalid update type");
			}
			}
		}

		m_pbIDToMap.insert({ pb->GetUniqueID() , mapType});

		return pb->GetUniqueID();
	}


	unordered_map<uint64_t, shared_ptr<ParameterBlock const>> const& ParameterBlockManager::GetImmutableParamBlocks() const
	{
		return m_immutablePBs;
	}


	unordered_map<uint64_t, shared_ptr<ParameterBlock>> const& ParameterBlockManager::GetMutableParamBlocks() const
	{
		return m_mutablePBs;
	}


	shared_ptr<ParameterBlock const> const ParameterBlockManager::GetParameterBlock(uint64_t pbID) const
	{
		auto result = m_pbIDToMap.find(pbID);
		SEAssert("Parameter block not found", result != m_pbIDToMap.end());
		switch (result->second)
		{
		case MapType::Immutable:
		{
			auto immutableResult = m_immutablePBs.find(pbID);
			SEAssert("Parameter block not found", immutableResult != m_immutablePBs.end());
			return immutableResult->second;
		}
		break;
		case MapType::Mutable:
		{
			auto mutableResult = m_mutablePBs.find(pbID);
			SEAssert("Parameter block not found", mutableResult != m_mutablePBs.end());
			return mutableResult->second;
		}
		break;
		case MapType::SingleFrame:
		{
			auto singleFrameResult = m_singleFramePBs.find(pbID);
			SEAssert("Parameter block not found", singleFrameResult != m_singleFramePBs.end());
			return singleFrameResult->second;
		}
		break;
		default:
			SEAssertF("Invalid map type");
		}

		return nullptr;
	}


	void ParameterBlockManager::UpdateParamBlocks()
	{
		for (auto pb : m_mutablePBs) // Immutable and single-frame PBs are buffered at creation
		{
			if (pb.second->GetDirty())
			{
				platform::ParameterBlock::Update(*pb.second.get());
			}
		}
	}


	void ParameterBlockManager::EndOfFrame()
	{
		m_singleFramePBs.clear();
	}
}