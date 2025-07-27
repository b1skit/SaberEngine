// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/CHashKey.h" // Required for DrawStyles.h
#include "Core/Util/HashKey.h"

#include "_generated/DrawStyles.h"

namespace core
{
	template<typename T>
	class InvPtr;
}
namespace effect
{
	class Effect;
	class EffectDB;
	class Technique;
}
namespace re
{
	class Shader;
}

struct EffectID // Wrapper around util::HashKey, with a EffectDB getter
{
	EffectID() noexcept = default;
	EffectID(util::HashKey const& hashKey) noexcept : m_effectID(hashKey) {}
	EffectID(util::HashKey&& hashKey) noexcept : m_effectID(std::move(hashKey)) {}
	EffectID(uint64_t hash) noexcept : m_effectID(hash) {}
	EffectID(char const* const cStr) noexcept : m_effectID(cStr) {}
	EffectID(std::string const& str) noexcept : m_effectID(str) {}

	EffectID(EffectID const&) noexcept = default;
	EffectID(EffectID&&) noexcept = default;
	EffectID& operator=(EffectID const&) noexcept = default;
	EffectID& operator=(EffectID&&) noexcept = default;

	EffectID& operator=(uint64_t hash) noexcept { m_effectID = hash; return *this; }
	EffectID& operator=(int zeroInit) noexcept { m_effectID = util::HashKey(zeroInit); return *this; }

	operator uint64_t() const noexcept { return m_effectID; }

	// Implicit conversions to/from HashKey
	operator util::HashKey& () noexcept { return m_effectID; }
	operator util::HashKey const& () const noexcept { return m_effectID; }

	bool operator==(EffectID const& rhs) const noexcept { return m_effectID == rhs.m_effectID; }
	bool operator!=(EffectID const& rhs) const noexcept { return m_effectID != rhs.m_effectID; }
	bool operator<(EffectID const& rhs) const noexcept { return m_effectID < rhs.m_effectID; }
	bool operator>(EffectID const& rhs) const noexcept { return m_effectID > rhs.m_effectID; }

	bool operator==(int rhs) const noexcept { return m_effectID == rhs; }
	bool operator!=(int rhs) const noexcept { return m_effectID != rhs; }

	effect::Effect const* GetEffect() const noexcept;
	effect::Technique const* GetTechnique(effect::drawstyle::Bitmask drawStyleBitmask) const;

	core::InvPtr<re::Shader> const& GetResolvedShader(effect::drawstyle::Bitmask drawStyleBitmask) const;

	util::HashKey const& GetEffectIDHashKey() const noexcept { return m_effectID; }


private:
	util::HashKey m_effectID;


private:
	friend class effect::EffectDB;
	static effect::EffectDB* s_effectDB;
};


namespace effect::drawstyle
{
	// Helpers for "_generated/DrawStyles.h":
	drawstyle::Bitmask GetDrawStyleBitmaskByName(std::string const& drawstyleName, std::string const& mode);
	std::string GetNamesFromDrawStyleBitmask(drawstyle::Bitmask bitmask); // Debug helper: Convert a bitmask back to a list of names
}


namespace effect
{
	class Effect final : public virtual core::INamedObject
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
		std::unordered_map<effect::drawstyle::Bitmask, effect::Technique const*> const& GetAllTechniques() const;
		bool UsesBuffer(util::HashKey bufferNameHash) const;
		std::map<util::HashKey, std::string> const& GetRequestedBufferShaderNames() const;


	public:
		void AddTechnique(effect::drawstyle::Bitmask, effect::Technique const*);
		void AddBufferName(std::string const& bufferShaderName);


	private:
		std::unordered_map<effect::drawstyle::Bitmask, effect::Technique const*> m_techniques;

		// Opt-in: A Effect can optionally associate itself with buffers by shader name
		std::map<util::HashKey, std::string> m_requestedBufferShaderNames;


	private:
		Effect(Effect const&) = delete;
		Effect& operator=(Effect const&) = delete;
	};


	inline EffectID Effect::ComputeEffectID(std::string const& effectName)
	{
		return util::HashKey(effectName);
	}


	inline EffectID Effect::GetEffectID() const
	{
		return GetNameHash();
	}


	inline Technique const* Effect::GetResolvedTechnique(effect::drawstyle::Bitmask drawStyleBitmask) const
	{
		SEAssert(m_techniques.contains(drawStyleBitmask),
			std::format("No Technique matches the Bitmask {}: \"{}\"",
				drawStyleBitmask,
				effect::drawstyle::GetNamesFromDrawStyleBitmask(drawStyleBitmask)).c_str());

		return m_techniques.at(drawStyleBitmask);
	}

	
	inline std::unordered_map<effect::drawstyle::Bitmask, effect::Technique const*> const& Effect::GetAllTechniques() const
	{
		return m_techniques;
	}


	inline bool Effect::UsesBuffer(util::HashKey bufferNameHash) const
	{
		SEAssert(bufferNameHash != 0, "Invalid buffer name hash");
		return m_requestedBufferShaderNames.contains(bufferNameHash);
	}


	inline std::map<util::HashKey, std::string> const& Effect::GetRequestedBufferShaderNames() const
	{
		return m_requestedBufferShaderNames;
	}
}


template <>
struct std::hash<EffectID>
{
	std::size_t operator()(EffectID const& effectID) const noexcept
	{
		return std::hash<util::HashKey>()(effectID.GetEffectIDHashKey());
	}
};


template <>
struct std::formatter<EffectID>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
	auto format(EffectID const& effectID, FormatContext& ctx) const
	{
		return std::format_to(ctx.out(), "{}", effectID.GetEffectIDHashKey());
	}
};