// © 2024 Adam Badke. All rights reserved.
#include "EffectDB.h"
#include "Platform.h"
#include "RenderManager.h"

#include "Core\Assert.h"
#include "Core\Config.h"

#include "Core\Definitions\ConfigKeys.h"


namespace
{
	// Effect Manifest:
	//-----------------
	constexpr char const* key_effectsBlock = "Effects";


	// Effect definitions:
	//--------------------

	// Common:
	constexpr char const* key_name = "Name";
	constexpr char const* key_excludedPlatforms = "ExcludedPlatforms";

	// "Effect":
	constexpr char const* key_effectBlock = "Effect";
	constexpr char const* key_parents = "Parents";	
	constexpr char const* key_defaultTechnique = "DefaultTechnique";
	constexpr char const* key_drawStyles = "DrawStyles";

	// "DrawStyles":
	constexpr char const* key_conditions = "Conditions";
	constexpr char const* key_rule = "Rule";
	constexpr char const* key_mode = "Mode";
	constexpr char const* key_technique = "Technique";

	// "PipelineStates":
	constexpr char const* key_pipelineStatesBlock = "PipelineStates";
	constexpr char const* key_topologyType = "TopologyType";
	constexpr char const* key_fillMode = "FillMode";
	constexpr char const* key_faceCullingMode = "FaceCullingMode";
	constexpr char const* key_windingOrder = "WindingOrder";
	constexpr char const* key_depthTestMode = "DepthTestMode";
	
	// "Techniques":
	constexpr char const* key_techniques = "Techniques";
	constexpr char const* key_pipelineState = "PipelineState";

	constexpr char const* keys_shaderTypes[] =
	{
		"VShader",
		"GShader",
		"PShader",
		"HShader",
		"DShader",
		"MShader",
		"AShader",
		"CShader",
	};
	SEStaticAssert(_countof(keys_shaderTypes) == re::Shader::ShaderType_Count,
		"Shader types and technique names are out of sync");


	// ---


	void ParseDrawStyleConditionEntry(
		auto const& drawStyleEntry,
		effect::EffectDB const& effectDB,
		effect::DrawStyle::Bitmask& drawStyleBitmaskOut,
		effect::Technique const*& resolvedTechniqueOut)
	{
		SEAssert(drawStyleEntry.contains(key_conditions) &&
			!drawStyleEntry.at(key_conditions).empty() &&
			drawStyleEntry.contains(key_technique),
			"Malformed DrawStyles block");

		drawStyleBitmaskOut = 0;
		for (auto const& condition : drawStyleEntry.at(key_conditions))
		{
			SEAssert(condition.contains(key_rule) && condition.contains(key_mode),
				"Malformed Conditions block entry");

			std::string const& ruleName = condition.at(key_rule).template get<std::string>();
			std::string const& modeName = condition.at(key_mode).template get<std::string>();

			drawStyleBitmaskOut |= effect::DrawStyle::GetDrawStyleBitmaskByName(ruleName, modeName);
		}

		std::string const& techniqueName = drawStyleEntry.at(key_technique).template get<std::string>();
		const TechniqueID techniqueID = effect::Technique::ComputeTechniqueID(techniqueName);
		
		resolvedTechniqueOut = effectDB.GetTechnique(techniqueID);
	}


	effect::Effect ParseJSONEffectBlock(
		auto const& effectBlock,
		effect::EffectDB const& effectDB,
		std::unordered_set<TechniqueID> const& excludedTechniques)
	{
		// "Name": Create an Effect
		effect::Effect newEffect(effectBlock.at(key_name).template get<std::string>().c_str());

		// "DefaultTechnique":
		if (effectBlock.contains(key_defaultTechnique))
		{
			std::string const& defaultTechniqueName = effectBlock.at(key_defaultTechnique).template get<std::string>();
			const TechniqueID defaultTechniqueID = effect::Technique::ComputeTechniqueID(defaultTechniqueName);

			if (!excludedTechniques.contains(defaultTechniqueID))
			{
				newEffect.AddTechnique(
					effect::DrawStyle::k_defaultTechniqueBitmask, effectDB.GetTechnique(defaultTechniqueID));
			}
		}

		// "DrawStyles":
		if (effectBlock.contains(key_drawStyles) && !effectBlock.at(key_drawStyles).empty())
		{
			for (auto const& drawStyleEntry : effectBlock.at(key_drawStyles))
			{
				effect::DrawStyle::Bitmask drawStyleBitmask(0);
				effect::Technique const* technique(nullptr);

				ParseDrawStyleConditionEntry(drawStyleEntry, effectDB, drawStyleBitmask, technique);
				SEAssert(drawStyleBitmask != 0, "DrawStyle bitmask is zero. This is unexpected");
				SEAssert(technique != nullptr, "Technique pointer is null. This is unexpected");

				if (!excludedTechniques.contains(technique->GetTechniqueID()))
				{
					newEffect.AddTechnique(drawStyleBitmask, technique);
				}
			}
		}

		return newEffect;
	}


	re::PipelineState ParsePipelineStateEntry(auto const& piplineStateEntry)
	{
		// Create a new PipelineState, and update it as necessary:
		re::PipelineState newPipelineState;

		// "TopologyType":
		if (piplineStateEntry.contains(key_topologyType))
		{
			newPipelineState.SetTopologyType(re::PipelineState::GetTopologyTypeByName(
				piplineStateEntry.at(key_topologyType).template get<std::string>().c_str()));
		}

		// "FillMode":
		if (piplineStateEntry.contains(key_fillMode))
		{
			newPipelineState.SetFillMode(re::PipelineState::GetFillModeByName(
				piplineStateEntry.at(key_fillMode).template get<std::string>().c_str()));
		}

		// "FaceCullingMode":
		if (piplineStateEntry.contains(key_faceCullingMode))
		{
			newPipelineState.SetFaceCullingMode(re::PipelineState::GetFaceCullingModeByName(
				piplineStateEntry.at(key_faceCullingMode).template get<std::string>().c_str()));
		}

		// "WindingOrder":
		if (piplineStateEntry.contains(key_windingOrder))
		{
			newPipelineState.SetWindingOrder(re::PipelineState::GetWindingOrderByName(
				piplineStateEntry.at(key_windingOrder).template get<std::string>().c_str()));
		}

		// "DepthTestMode":
		if (piplineStateEntry.contains(key_depthTestMode))
		{
			newPipelineState.SetDepthTestMode(re::PipelineState::GetDepthTestModeByName(
				piplineStateEntry.at(key_depthTestMode).template get<std::string>().c_str()));
		}

		return newPipelineState;
	}


	effect::Technique ParseJSONTechniqueEntry(
		auto const& techniqueEntry,
		effect::EffectDB const& effectDB)
	{
		SEAssert(techniqueEntry.contains(key_name), "Incomplete Technique definition");

		// "Name": Create a new Technique called "OwningEffectName::TechniqueName":
		std::string const& techniqueName = techniqueEntry.at(key_name).template get<std::string>();

		// "*Shader" names:
		std::vector<std::pair<std::string, re::Shader::ShaderType>> shaderNames;
		for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
		{
			if (techniqueEntry.contains(keys_shaderTypes[shaderTypeIdx]))
			{
				shaderNames.emplace_back(
					techniqueEntry.at(keys_shaderTypes[shaderTypeIdx]).template get<std::string>(),
					static_cast<re::Shader::ShaderType>(shaderTypeIdx));
			}
		}

		SEAssert(techniqueEntry.contains(key_pipelineState),
			"Failed to find PipelineState entry. This is (currently) required");

		std::string const& pipelineStateName = techniqueEntry.at(key_pipelineState).template get<std::string>();

		re::PipelineState const* pipelineState = effectDB.GetPipelineState(pipelineStateName);

		return effect::Technique(techniqueName.c_str(), shaderNames, pipelineState);
	}
}


namespace effect
{
	EffectDB::~EffectDB()
	{
		SEAssert(m_effects.empty() && m_techniques.empty(), 
			"EffectDB is being deconstructed before Destroy() was called");
	}


	void EffectDB::Destroy()
	{
		{
			std::scoped_lock lock(m_effectsMutex, m_techniquesMutex);

			m_effects.clear();
			m_techniques.clear();
		}
	}


	void EffectDB::LoadEffectManifest()
	{
		std::string const& effectManifestFilepath =
			std::format("{}{}", core::configkeys::k_effectDirName, core::configkeys::k_effectManifestFilename);

		LOG("Loading Effect manifest \"%s\"...", effectManifestFilepath.c_str());

		std::ifstream effectManifestInputStream(effectManifestFilepath);
		SEAssert(effectManifestInputStream.is_open(), "Failed to open effect manifest input stream");

		const bool allowExceptions = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonAllowExceptionsKey);
		const bool ignoreComments = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonIgnoreCommentsKey);

		nlohmann::json effectManifestJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			effectManifestJSON = 
				nlohmann::json::parse(effectManifestInputStream, parserCallback, allowExceptions, ignoreComments);

			SEAssert(effectManifestJSON.contains(key_effectsBlock) && !effectManifestJSON.at(key_effectsBlock).empty(),
				"Malformed effects manifest");

			for (auto const& effectManifestEntry : effectManifestJSON.at(key_effectsBlock))
			{
				std::string const& effectDefinitionFilename = effectManifestEntry.template get<std::string>();
				LoadEffect(effectDefinitionFilename);
			}
		}
		catch (nlohmann::json::parse_error parseException)
		{
			std::string const& error = std::format(
				"Failed to parse the Effect manifest file \"{}\"\n{}",
				effectManifestFilepath,
				parseException.what());
			SEAssertF(error.c_str());
		}
	}


	void EffectDB::LoadEffect(std::string const& effectName)
	{
		const EffectID effectID = effect::Effect::ComputeEffectID(effectName);
		if (HasEffect(effectID)) // Only process new Effects
		{
			return;
		}

		constexpr char const* k_effectDefinitionFileExtension = ".json";
		std::string const& effectFilepath = 
			std::format("{}{}{}", core::configkeys::k_effectDirName, effectName, k_effectDefinitionFileExtension);

		LOG("Loading Effect \"%s\"...", effectFilepath.c_str());

		std::ifstream effectInputStream(effectFilepath);
		SEAssert(effectInputStream.is_open(), "Failed to open Effect definition input stream");

		const bool allowExceptions = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonAllowExceptionsKey);
		const bool ignoreComments = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonIgnoreCommentsKey);

		std::string const& currentPlatformVal = platform::RenderingAPIToCStr(re::RenderManager::Get()->GetRenderingAPI());
		auto ExcludesPlatform = [&currentPlatformVal = std::as_const(currentPlatformVal)](auto entry) -> bool
			{
				if (entry.contains(key_excludedPlatforms))
				{
					for (auto const& excludedPlatform : entry[key_excludedPlatforms])
					{
						if (excludedPlatform.template get<std::string>() == currentPlatformVal)
						{
							return true;
						}
					}
				}
				return false;
			};

		nlohmann::json effectJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			effectJSON = nlohmann::json::parse(effectInputStream, parserCallback, allowExceptions, ignoreComments);

			// Peek ahead at critical Effect properties, we'll load the rest of the Effect block later
			if (effectJSON.contains(key_effectBlock))
			{
				auto const& effectBlock = effectJSON.at(key_effectBlock);

				// "Parents": Parsed first to ensure dependencies exist
				if (effectBlock.contains(key_parents) &&
					!effectBlock.at(key_parents).empty())
				{
					for (auto const& parent : effectBlock.at(key_parents))
					{
						std::string const& parentName = parent.template get<std::string>();
						LoadEffect(parentName);
					}
				}
			}

			// "PipelineStates":
			if (effectJSON.contains(key_pipelineStatesBlock) && !effectJSON.at(key_pipelineStatesBlock).empty())
			{
				auto const& pipelineStateBlock = effectJSON.at(key_pipelineStatesBlock);
				for (auto const& piplineStateEntry : pipelineStateBlock)
				{
					SEAssert(piplineStateEntry.contains(key_name), "Incomplete PipelineState definition");
					
					std::string const& pipelineStateName = piplineStateEntry.at(key_name).template get<std::string>();
					AddPipelineState(pipelineStateName, ParsePipelineStateEntry(piplineStateEntry));
				}
			}

			// "Techniques":
			std::unordered_set<TechniqueID> excludedTechniques;
			if (effectJSON.contains(key_techniques) && !effectJSON.at(key_techniques).empty())
			{
				// "Techniques":
				for (auto const& techniqueEntry : effectJSON.at(key_techniques))
				{
					std::string const& techniqueName = techniqueEntry.at(key_name).template get<std::string>();

					// "ExcludedPlatforms": Skip this technique if it is excluded
					if (ExcludesPlatform(techniqueEntry))
					{
						continue;
					}
					AddTechnique(ParseJSONTechniqueEntry(techniqueEntry, *this));
				}				
			}

			// "Effect":
			if (effectJSON.contains(key_effectBlock))
			{
				auto const& effectBlock = effectJSON.at(key_effectBlock);

				SEAssert(effectBlock.contains(key_name) && 
					effectName == effectBlock.at(key_name).template get<std::string>(),
					"Effect name and effect definition filename do not match. This is unexpected");

				// "ExcludedPlatforms":
				if (ExcludesPlatform(effectBlock))
				{
					LOG("Effect \"%s\" is excluded on the \"%s\" platform. Skipping.",
						effectFilepath.c_str(),
						currentPlatformVal.c_str());
				}
				else
				{
					AddEffect(ParseJSONEffectBlock(effectBlock, *this, excludedTechniques));
				}
			}
		}
		catch (nlohmann::json::parse_error parseException)
		{
			std::string const& error = std::format(
				"Failed to parse the effect file \"{}\"\n{}",
				effectFilepath,
				parseException.what());
			SEAssertF(error.c_str());
		}
	}


	bool EffectDB::HasEffect(EffectID effectID) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_effectsMutex);
			return m_effects.contains(effectID);
		}
	}


	effect::Effect* EffectDB::AddEffect(effect::Effect&& newEffect)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_effectsMutex);

			if (m_effects.contains(newEffect.GetEffectID()))
			{
				SEAssert(m_effects.at(newEffect.GetEffectID()) == newEffect,
					"An Effect with the same name but different configuration exists. Effect names must be unique");

				return &m_effects.at(newEffect.GetEffectID());
			}

			auto result = m_effects.emplace(newEffect.GetEffectID(), std::move(newEffect));
			
			return &(result.first->second);
		}
	}


	bool EffectDB::HasTechnique(TechniqueID techniqueID) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_techniquesMutex);
			return m_techniques.contains(techniqueID);
		}
	}


	effect::Technique* EffectDB::AddTechnique(effect::Technique&& newTechnique)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_techniquesMutex);

			if (m_techniques.contains(newTechnique.GetTechniqueID()))
			{
				SEAssert(m_techniques.at(newTechnique.GetTechniqueID()) == newTechnique,
					"A Technique with the given name but different configuration exists. Technique names must be unique");

				return &m_techniques.at(newTechnique.GetTechniqueID());
			}

			auto result = m_techniques.emplace(newTechnique.GetTechniqueID(), std::move(newTechnique));

			return &(result.first->second);
		}
	}


	bool EffectDB::HasPipelineState(std::string const& name) const
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_pipelineStatesMutex);
			return m_pipelineStates.contains(name);
		}
	}


	re::PipelineState* EffectDB::AddPipelineState(std::string const& name, re::PipelineState&& newPipelineState)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_pipelineStatesMutex);

			if (m_pipelineStates.contains(name))
			{
				SEAssert(m_pipelineStates.at(name).GetDataHash() == newPipelineState.GetDataHash(),
					"A PipelineState with the given name but different data hash exists. Names must be unique");

				return &m_pipelineStates.at(name);
			}

			auto result = m_pipelineStates.emplace(name, std::move(newPipelineState));

			return &(result.first->second);
		}
	}
}