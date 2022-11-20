#pragma once

#include <unordered_map>
#include <memory>

#include "ParameterBlock.h"


namespace re
{
	class ParameterBlockManager
	{
	public:
		ParameterBlockManager() = default;
		~ParameterBlockManager() = default;

		void UpdateParamBlocks();
		void EndOfFrame(); // Clears single-frame PBs

		ParameterBlock::Handle RegisterParameterBlock(std::shared_ptr<re::ParameterBlock> pb);

		std::unordered_map<ParameterBlock::Handle, std::shared_ptr<re::ParameterBlock>> const& GetImmutableParamBlocks() const;
		std::unordered_map<ParameterBlock::Handle, std::shared_ptr<re::ParameterBlock>> const& GetMutableParamBlocks() const;

		std::shared_ptr<re::ParameterBlock> GetParameterBlock(ParameterBlock::Handle pbID) const;

		template <typename T>
		void SetData(ParameterBlock::Handle pbID, T const& data);


	private:
		enum class MapType
		{
			Immutable,
			Mutable,
			SingleFrame,
			MapType_Count
		};

		std::unordered_map<ParameterBlock::Handle, std::shared_ptr<re::ParameterBlock>> m_immutablePBs;
		std::unordered_map<ParameterBlock::Handle, std::shared_ptr<re::ParameterBlock>> m_mutablePBs;
		std::unordered_map<ParameterBlock::Handle, std::shared_ptr<re::ParameterBlock>> m_singleFramePBs;

		std::unordered_map<ParameterBlock::Handle, MapType> m_pbIDToMap;

	private:
		ParameterBlockManager(ParameterBlockManager const&) = delete;
		ParameterBlockManager(ParameterBlockManager&&) = delete;
		ParameterBlockManager& operator=(ParameterBlockManager const&) = delete;
	};


	template <typename T>
	void ParameterBlockManager::SetData(ParameterBlock::Handle pbID, T const& data)
	{
		std::shared_ptr<ParameterBlock> pb = GetParameterBlock(pbID);
		SEAssert("Parameter block pointer is null", pb != nullptr);
		SEAssert("Cannot set data of an immutable param block", 
			pb->GetUpdateType() != ParameterBlock::UpdateType::Immutable);
		pb->SetData(data);
	}
}
