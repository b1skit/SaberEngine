// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"

#include "Core/Interfaces/INamedObject.h"


namespace effect
{
	class EffectDB
	{
	public:
		EffectDB() = default;
		EffectDB(EffectDB&&) noexcept = default;
		EffectDB& operator=(EffectDB&&) noexcept = default;
		~EffectDB();

		void Destroy();

		void LoadEffectManifest();

		effect::Effect const* GetEffect(EffectID) const;
		effect::Technique const* GetTechnique(TechniqueID) const;
		re::PipelineState const* GetPipelineState(std::string const&) const;
		re::VertexStreamMap const* GetVertexStreamMap(std::string const&) const;


	private:
		effect::Effect const* LoadEffect(std::string const&);

		bool HasEffect(EffectID) const;
		effect::Effect* AddEffect(effect::Effect&&);

		bool HasTechnique(TechniqueID) const;
		effect::Technique* AddTechnique(effect::Technique&&);

		bool HasPipelineState(std::string const& name) const;
		re::PipelineState* AddPipelineState(std::string const& name, re::PipelineState&&);

		bool HasVertexStreamMap(std::string const& name) const;
		re::VertexStreamMap* AddVertexStreamMap(std::string const& name, re::VertexStreamMap const&);


	private:
		std::unordered_map<EffectID, effect::Effect> m_effects;
		mutable std::shared_mutex m_effectsMutex;
		
		std::unordered_map<TechniqueID, effect::Technique> m_techniques;
		mutable std::shared_mutex m_techniquesMutex;

		std::unordered_map<std::string, re::PipelineState> m_pipelineStates;
		mutable std::shared_mutex m_pipelineStatesMutex;

		std::unordered_map<std::string, re::VertexStreamMap> m_vertexStreamMaps;
		mutable std::shared_mutex m_vertexStreamMapsMutex;


	private: // No copying allowed
		EffectDB(EffectDB const&) = delete;
		EffectDB& operator=(EffectDB const&) = delete;
	};


	inline effect::Effect const* EffectDB::GetEffect(EffectID effectID) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_effectsMutex);

			SEAssert(m_effects.contains(effectID), std::format("No Effect with ID {} exists", effectID).c_str());

			return &m_effects.at(effectID);
		}
	}


	inline effect::Technique const* EffectDB::GetTechnique(TechniqueID techniqueID) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_techniquesMutex);

			SEAssert(m_techniques.contains(techniqueID),
				"No Technique with the given ID exists");

			return &m_techniques.at(techniqueID);
		}
	}


	inline re::PipelineState const* EffectDB::GetPipelineState(std::string const& pipelineStateName) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_pipelineStatesMutex);

			SEAssert(m_pipelineStates.contains(pipelineStateName),
				"No PipelineState with the given name exists");

			return &m_pipelineStates.at(pipelineStateName);
		}
	}


	inline re::VertexStreamMap const* EffectDB::GetVertexStreamMap(std::string const& name) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_vertexStreamMapsMutex);

			SEAssert(m_vertexStreamMaps.contains(name),
				"No VertexStreamMap is associated with the given name");

			return &m_vertexStreamMaps.at(name);
		}
	}
}