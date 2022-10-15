#include "ParameterBlockManager.h"
#include "DebugConfiguration.h"
#include "ParameterBlock_Platform.h"

using std::shared_ptr;
using re::ParameterBlock;


namespace re
{
	size_t ParameterBlockManager::RegisterParameterBlock(std::shared_ptr<re::ParameterBlock> pb)
	{
		if (pb->GetLifetime() == re::ParameterBlock::Lifetime::SingleFrame)
		{
			SEAssert("Parameter block is already registered", !m_singleFramePBs.contains(pb->GetUniqueID()));
			m_singleFramePBs[pb->GetUniqueID()] = pb;
		}
		else
		{
			switch (pb->GetUpdateType())
			{
			case ParameterBlock::UpdateType::Immutable:
			{
				SEAssert("Parameter block is already registered", !m_immutablePBs.contains(pb->GetUniqueID()));
				m_immutablePBs[pb->GetUniqueID()] = pb;
			}
			break;
			case ParameterBlock::UpdateType::Mutable:
			{
				SEAssert("Parameter block is already registered", !m_mutablePBs.contains(pb->GetUniqueID()));
				m_mutablePBs[pb->GetUniqueID()] = pb;
			}
			break;
			default:
			{
				SEAssert("Invalid update type", false);
			}
			}
		}

		return pb->GetUniqueID();
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