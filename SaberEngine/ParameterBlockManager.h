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
		void EndOfFrame();
		// TODO: Should ParameterBlockManager be an updateable?
		// -> Updateable could have an opt-in EndOfFrame() member...?

		size_t RegisterParameterBlock(std::shared_ptr<re::ParameterBlock> pb);

		inline std::unordered_map<size_t, std::shared_ptr<re::ParameterBlock const>> const& GetImmutableParamBlocks() const 
			{ return m_immutablePBs; }
		
		inline std::unordered_map<size_t, std::shared_ptr<re::ParameterBlock>> const& GetMutableParamBlocks() const 
			{ return m_mutablePBs; }

	private:
		// TODO: Is it optimal to maintain PBs in separate unordered_maps, or should they be combined together?
		std::unordered_map<size_t, std::shared_ptr<re::ParameterBlock const>> m_immutablePBs;
		std::unordered_map<size_t, std::shared_ptr<re::ParameterBlock>> m_mutablePBs;
		std::unordered_map<size_t, std::shared_ptr<re::ParameterBlock>> m_singleFramePBs;

	private:
		ParameterBlockManager(ParameterBlockManager const&) = delete;
		ParameterBlockManager(ParameterBlockManager&&) = delete;
		ParameterBlockManager& operator=(ParameterBlockManager const&) = delete;
	};
}
