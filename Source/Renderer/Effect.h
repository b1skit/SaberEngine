// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "PipelineState.h"
#include "Shader.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/HashKey.h"

#include "Generated/DrawStyles.h"


using EffectID = NameID;
using TechniqueID = NameID;


namespace effect::drawstyle
{
	// Helpers for "Generated/DrawStyles.h":
	drawstyle::Bitmask GetDrawStyleBitmaskByName(std::string const& drawstyleName, std::string const& mode);
	std::string GetNamesFromDrawStyleBitmask(drawstyle::Bitmask bitmask); // Debug helper: Convert a bitmask back to a list of names
}


namespace effect
{
	class Technique;


	class Effect : public virtual core::INamedObject
	{
	public:
		static constexpr EffectID k_invalidEffectID = core::INamedObject::k_invalidNameID;

		static EffectID ComputeEffectID(std::string const& effectName);

	public:
		Effect(char const* name);

		Effect(Effect&&) = default;
		Effect& operator=(Effect&&) = default;

		~Effect() = default;

		bool operator==(Effect const&) const;


	public:
		EffectID GetEffectID() const;

		Technique const* GetResolvedTechnique(effect::drawstyle::Bitmask) const;


	public:
		void AddTechnique(effect::drawstyle::Bitmask, effect::Technique const*);


	private:
		std::unordered_map<effect::drawstyle::Bitmask, effect::Technique const*> m_techniques;


	private:
		Effect(Effect const&) = delete;
		Effect& operator=(Effect const&) = delete;
	};


	inline EffectID Effect::ComputeEffectID(std::string const& effectName)
	{
		return core::INamedObject::ComputeIDFromName(effectName);
	}


	inline EffectID Effect::GetEffectID() const
	{
		return GetNameID();
	}


	inline Technique const* Effect::GetResolvedTechnique(effect::drawstyle::Bitmask drawStyleBitmask) const
	{
		SEAssert(m_techniques.contains(drawStyleBitmask),
			std::format("No Technique matches the given Bitmask: {}", 
				effect::drawstyle::GetNamesFromDrawStyleBitmask(drawStyleBitmask)).c_str());

		return m_techniques.at(drawStyleBitmask);
	}


	// ---


	class Technique : public virtual core::INamedObject
	{
	public:
		static constexpr EffectID k_invalidTechniqueID = core::INamedObject::k_invalidNameID;

		static TechniqueID ComputeTechniqueID(std::string const& techniqueName);


	public:
		Technique(
			char const* name, 
			std::vector<std::pair<std::string, re::Shader::ShaderType>> const&,
			re::PipelineState const*,
			re::VertexStreamMap const*);
		
		Technique(Technique&&) = default;
		Technique& operator=(Technique&&) = default;

		~Technique() = default;

		bool operator==(Technique const&) const;


	public:
		TechniqueID GetTechniqueID() const;

		re::Shader const* GetShader() const;


	private:	
		std::shared_ptr<re::Shader> m_resolvedShader;


	private: // No copying allowed
		Technique(Technique const&) = delete;
		Technique& operator=(Technique const&) = delete;
	};


	inline TechniqueID Technique::ComputeTechniqueID(std::string const& techniqueName)
	{
		return core::INamedObject::ComputeIDFromName(techniqueName);
	}


	inline re::Shader const* Technique::GetShader() const
	{
		return m_resolvedShader.get();
	}
}