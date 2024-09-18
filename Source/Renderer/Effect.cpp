// © 2024 Adam Badke. All rights reserved.
#include "Effect.h"

#include "Core/Assert.h"
#include "Core/Definitions/ConfigKeys.h"


namespace effect::drawstyle
{
	drawstyle::Bitmask GetDrawStyleBitmaskByName(std::string const& drawstyleName, std::string const& mode)
	{
		effect::drawstyle::DrawStyleRuleToModes const& drawstyleBitmaskMappings =
			effect::drawstyle::GetDrawStyleRuleToModesMap();

		SEAssert(drawstyleBitmaskMappings.contains(util::HashKey::Create(drawstyleName)) &&
			drawstyleBitmaskMappings.at(util::HashKey::Create(drawstyleName)).contains(util::HashKey::Create(mode)),
			"Draw style name or mode name not found");

		return drawstyleBitmaskMappings.at(util::HashKey::Create(drawstyleName)).at(util::HashKey::Create(mode));
	}


	std::string GetNamesFromDrawStyleBitmask(drawstyle::Bitmask bitmask)
	{
		using BitmaskToEffectAndMode = std::unordered_map<effect::drawstyle::Bitmask, std::string>;

		// Build a static reverse lookup map
		static BitmaskToEffectAndMode s_drawstyleBitmaskMappings;

		static std::atomic<bool> s_isInitialized = false;
		if (!s_isInitialized)
		{
			static std::mutex s_initializationMutex;

			{
				std::lock_guard<std::mutex> lock(s_initializationMutex);

				if (!s_isInitialized)
				{
					s_isInitialized.store(true);

					effect::drawstyle::DrawStyleRuleToModes const& drawstyleBitmaskMappings =
						effect::drawstyle::GetDrawStyleRuleToModesMap();

					for (auto const& effectEntry : drawstyleBitmaskMappings)
					{
						char const* effectName = effectEntry.first.GetKey();

						for (auto const& modeBitmaskEntry : effectEntry.second)
						{
							char const* modeName = modeBitmaskEntry.first.GetKey();
							const effect::drawstyle::Bitmask bitmask = modeBitmaskEntry.second;

							s_drawstyleBitmaskMappings.emplace(bitmask, std::format("{}::{}", effectName, modeName));
						}
					}
				}
			}
		}

		// Concatenate the results:
		std::string names;
		constexpr uint8_t k_numBits = sizeof(drawstyle::Bitmask) * 8;
		for (uint8_t bitIdx = 0; bitIdx < k_numBits; ++bitIdx)
		{
			const drawstyle::Bitmask curBit = drawstyle::Bitmask(1) << bitIdx;
			if (bitmask & curBit)
			{
				if (!names.empty())
				{
					names += "|";
				}
				names += s_drawstyleBitmaskMappings.at(curBit);
			}
		}
		return names;
	}
}


namespace effect
{
	Effect::Effect(char const* name)
		: INamedObject(name)
	{
	}


	bool Effect::operator==(Effect const& rhs) const
	{
		if (this == &rhs)
		{
			return true;
		}

		const bool isSame = GetEffectID() == rhs.GetEffectID();

		SEAssert(!isSame || m_techniques == rhs.m_techniques, 
			"Found an Effect with the same name but different Techniques");

		return  isSame;
	}


	void Effect::AddTechnique(effect::drawstyle::Bitmask drawStyleBitmask, effect::Technique const* technique)
	{
		SEAssert(!m_techniques.contains(drawStyleBitmask),
			"A Technique has already been added for the given draw style bitmask");

		m_techniques.emplace(drawStyleBitmask, technique);
	}
}