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

		uint64_t RegisterParameterBlock(std::shared_ptr<re::ParameterBlock> pb);

		std::unordered_map<uint64_t, std::shared_ptr<re::ParameterBlock const>> const& GetImmutableParamBlocks() const;
		std::unordered_map<uint64_t, std::shared_ptr<re::ParameterBlock>> const& GetMutableParamBlocks() const;
		std::shared_ptr<re::ParameterBlock const> const GetParameterBlock(uint64_t pbID) const;

	private:
		enum class MapType
		{
			Immutable,
			Mutable,
			SingleFrame,
			MapType_Count
		};

		// TODO: Is it optimal to maintain PBs in separate unordered_maps, or should they be combined together?
		std::unordered_map<uint64_t, std::shared_ptr<re::ParameterBlock const>> m_immutablePBs;
		std::unordered_map<uint64_t, std::shared_ptr<re::ParameterBlock>> m_mutablePBs;
		std::unordered_map<uint64_t, std::shared_ptr<re::ParameterBlock>> m_singleFramePBs;

		std::unordered_map<uint64_t, MapType> m_pbIDToMap;

	private:
		ParameterBlockManager(ParameterBlockManager const&) = delete;
		ParameterBlockManager(ParameterBlockManager&&) = delete;
		ParameterBlockManager& operator=(ParameterBlockManager const&) = delete;
	};
}
