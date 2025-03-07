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
		effect::Technique const* GetTechnique(EffectID, effect::drawstyle::Bitmask drawStyleBitmask) const;

		re::RasterizationState const* GetRasterizationState(std::string const&) const;
		re::VertexStreamMap const* GetVertexStreamMap(std::string const&) const;
		
		core::InvPtr<re::Shader> const& GetResolvedShader(
			EffectID effectID, effect::drawstyle::Bitmask drawStyleBitmask) const;


	private:
		effect::Effect const* LoadEffect(std::string const&);

		bool HasEffect(EffectID) const;
		effect::Effect* AddEffect(effect::Effect&&);

		bool HasTechnique(TechniqueID) const;
		effect::Technique* AddTechnique(effect::Technique&&);

		bool HasRasterizationState(std::string const& name) const;
		re::RasterizationState* AddRasterizationState(std::string const& name, re::RasterizationState&&);

		bool HasVertexStreamMap(std::string const& name) const;
		re::VertexStreamMap* AddVertexStreamMap(std::string const& name, re::VertexStreamMap const&);


	private:
		std::unordered_map<EffectID, effect::Effect> m_effects;
		mutable std::shared_mutex m_effectsMutex;
		
		std::unordered_map<TechniqueID, effect::Technique> m_techniques;
		mutable std::shared_mutex m_techniquesMutex;

		std::unordered_map<std::string, re::RasterizationState> m_rasterizationStates;
		mutable std::shared_mutex m_rasterizationStatesMutex;

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


	inline effect::Technique const* EffectDB::GetTechnique(
		EffectID effectID, effect::drawstyle::Bitmask drawStyleBitmask) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_effectsMutex);

			SEAssert(m_effects.contains(effectID), std::format("No Effect with ID {} exists", effectID).c_str());

			effect::Effect const& effect = m_effects.at(effectID);

			return effect.GetResolvedTechnique(drawStyleBitmask);
		}
	}


	inline re::RasterizationState const* EffectDB::GetRasterizationState(std::string const& rasterStateName) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_rasterizationStatesMutex);

			SEAssert(m_rasterizationStates.contains(rasterStateName),
				"No RasterizationState with the given name exists");

			return &m_rasterizationStates.at(rasterStateName);
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


	inline core::InvPtr<re::Shader> const& EffectDB::GetResolvedShader(
		EffectID effectID, effect::drawstyle::Bitmask drawStyleBitmask) const
	{
		SEAssert(effectID != 0, "Invalid Effect");

		effect::Effect const* effect = GetEffect(effectID);
		effect::Technique const* technique = effect->GetResolvedTechnique(drawStyleBitmask);
		return technique->GetShader();
	}
}