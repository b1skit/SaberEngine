// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "PipelineState.h"
#include "Shader.h"

#include "Core/Interfaces/INamedObject.h"

#include "Generated/DrawStyles.h"


using EffectID = NameID;
using TechniqueID = NameID;


namespace effect
{
	class Technique;
	class Effect;


	class DrawStyle
	{
	public:
		using ModeToBitmask = std::unordered_map<std::string, effect::drawstyle::Bitmask>;
		using DrawStyleRuleToModes = std::unordered_map<std::string, ModeToBitmask>;

	private:
		static DrawStyleRuleToModes const& GetDrawStyleRuleToModesMap()
		{
			static const DrawStyleRuleToModes s_drawstyleBitmaskMappings(
				{
					{
						"RenderPath",
						{
							{"Deferred", effect::drawstyle::RenderPath_Deferred},
							{"Forward", effect::drawstyle::RenderPath_Forward}
						}
					},
					{
						"MaterialAlphaMode",
						{
							{"Opaque", effect::drawstyle::MaterialAlphaMode_Opaque},
							{"Mask", effect::drawstyle::MaterialAlphaMode_Mask},
							{"Blend", effect::drawstyle::MaterialAlphaMode_Blend},
						}
					},
					{
						"MaterialSidedness",
						{
							{"Single", effect::drawstyle::MaterialSidedness_Single},
							{"Double", effect::drawstyle::MaterialSidedness_Double},
						}
					},
					{
						"Debug",
						{
							{"Line", effect::drawstyle::Debug_Line},
							{"Triangle", effect::drawstyle::Debug_Triangle},
						}
					},
					{
						"Bloom",
						{
							{"EmissiveBlit", effect::drawstyle::Bloom_EmissiveBlit},
						}
					},
					{
						"DeferredLighting",
						{
							{"BRDFIntegration", effect::drawstyle::DeferredLighting_BRDFIntegration},
							{"IEMGeneration", effect::drawstyle::DeferredLighting_IEMGeneration},
							{"PMREMGeneration", effect::drawstyle::DeferredLighting_PMREMGeneration},
							{"DeferredAmbient", effect::drawstyle::DeferredLighting_DeferredAmbient},
							{"DeferredDirectional", effect::drawstyle::DeferredLighting_DeferredDirectional},
							{"DeferredPoint", effect::drawstyle::DeferredLighting_DeferredPoint},
							{"DeferredSpot", effect::drawstyle::DeferredLighting_DeferredSpot},
						}
					},
					{
						"XeGTAO",
						{
							{"PrefilterDepths", effect::drawstyle::XeGTAO_PrefilterDepths},
							{"LowQuality", effect::drawstyle::XeGTAO_LowQuality},
							{"MedQuality", effect::drawstyle::XeGTAO_MedQuality},
							{"HighQuality", effect::drawstyle::XeGTAO_HighQuality},
							{"UltraQuality", effect::drawstyle::XeGTAO_UltraQuality},
							{"Denoise", effect::drawstyle::XeGTAO_Denoise},
							{"DenoiseLastPass", effect::drawstyle::XeGTAO_DenoiseLastPass},
						}
					},
					{
						"Shadow",
						{
							{"2D", effect::drawstyle::Shadow_2D},
							{"Cube", effect::drawstyle::Shadow_Cube},
						}
					},
					{
						"TextureDimension",
						{
							{"1D", effect::drawstyle::TextureDimension_1D},
							{"2D", effect::drawstyle::TextureDimension_2D},
							{"3D", effect::drawstyle::TextureDimension_3D},
						}
					},
				});
			return s_drawstyleBitmaskMappings;
		}


	public:
		static drawstyle::Bitmask GetDrawStyleBitmaskByName(std::string const& drawstyleName, std::string const& mode)
		{
			DrawStyleRuleToModes const& drawstyleBitmaskMappings = GetDrawStyleRuleToModesMap();

			SEAssert(drawstyleBitmaskMappings.contains(drawstyleName) && 
				drawstyleBitmaskMappings.at(drawstyleName).contains(mode),
				"Draw style name or mode name not found");

			return drawstyleBitmaskMappings.at(drawstyleName).at(mode);
		}


		// Debug helper: Convert a bitmask back to a list of names
		static std::string GetNamesFromDrawStyleBitmask(drawstyle::Bitmask bitmask)
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

						DrawStyleRuleToModes const& drawstyleBitmaskMappings = GetDrawStyleRuleToModesMap();

						for (auto const& effectEntry : drawstyleBitmaskMappings)
						{
							std::string const& effectName = effectEntry.first;

							for (auto const& modeBitmaskEntry : effectEntry.second)
							{
								std::string const& modeName = modeBitmaskEntry.first;
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
					names += s_drawstyleBitmaskMappings.at(curBit) + "|";
				}
			}
			return names;
		}
	};


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
				effect::DrawStyle::GetNamesFromDrawStyleBitmask(drawStyleBitmask)).c_str());

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

		std::shared_ptr<re::Shader> GetShader() const;


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


	inline std::shared_ptr<re::Shader> Technique::GetShader() const
	{
		return m_resolvedShader;
	}
}