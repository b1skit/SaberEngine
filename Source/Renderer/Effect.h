// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Technique.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/HashKey.h"

#include "Generated/DrawStyles.h"


using EffectID = util::StringHash;


namespace effect::drawstyle
{
	// Helpers for "Generated/DrawStyles.h":
	drawstyle::Bitmask GetDrawStyleBitmaskByName(std::string const& drawstyleName, std::string const& mode);
	std::string GetNamesFromDrawStyleBitmask(drawstyle::Bitmask bitmask); // Debug helper: Convert a bitmask back to a list of names
}


namespace effect
{
	class Effect : public virtual core::INamedObject
	{
	public:
		static EffectID ComputeEffectID(std::string const& effectName);

	public:
		Effect(char const* name);

		Effect(Effect&&) noexcept = default;
		Effect& operator=(Effect&&) noexcept = default;

		~Effect() = default;

		bool operator==(Effect const&) const;


	public:
		EffectID GetEffectID() const;

		Technique const* GetResolvedTechnique(effect::drawstyle::Bitmask) const;

		bool UsesBuffer(util::StringHash) const;


	public:
		void AddTechnique(effect::drawstyle::Bitmask, effect::Technique const*);

		void AddBufferName(util::StringHash);


	private:
		std::unordered_map<effect::drawstyle::Bitmask, effect::Technique const*> m_techniques;

		std::set<util::StringHash> m_buffers; // Opt-in: A Effect can optionally associate itself with buffers by name


	private:
		Effect(Effect const&) = delete;
		Effect& operator=(Effect const&) = delete;
	};


	inline EffectID Effect::ComputeEffectID(std::string const& effectName)
	{
		return util::StringHash(effectName);
	}


	inline EffectID Effect::GetEffectID() const
	{
		return GetNameHash();
	}


	inline Technique const* Effect::GetResolvedTechnique(effect::drawstyle::Bitmask drawStyleBitmask) const
	{
		SEAssert(m_techniques.contains(drawStyleBitmask),
			std::format("No Technique matches the given Bitmask: {}", 
				effect::drawstyle::GetNamesFromDrawStyleBitmask(drawStyleBitmask)).c_str());

		return m_techniques.at(drawStyleBitmask);
	}


	inline bool Effect::UsesBuffer(util::StringHash bufferNameHash) const
	{
		SEAssert(bufferNameHash.IsValid(), "Invalid buffer name hash");
		return m_buffers.contains(bufferNameHash);
	}
}