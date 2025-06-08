// � 2024 Adam Badke. All rights reserved.
#include "EffectDB.h"
#include "EffectKeys.h"
#include "EnumTypes.h"
#include "RenderManager.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/ThreadPool.h"

#include "Core/Util/CHashKey.h"
#include "Core/Util/TextUtils.h"

#include "Core/Definitions/ConfigKeys.h"

#include "_generated/DrawStyles.h"


namespace
{
	bool ExcludesPlatform(auto const& entry)
	{
		if (entry.contains(key_excludedPlatforms))
		{
			std::string const& currentPlatformVal =
				platform::RenderingAPIToCStr(re::RenderManager::Get()->GetRenderingAPI());

			for (auto const& excludedPlatform : entry[key_excludedPlatforms])
			{
				if (excludedPlatform.template get<std::string>() == currentPlatformVal)
				{
					return true;
				}
			}
		}
		return false;
	}


	void ParseDrawStyleConditionEntry(
		auto const& drawStyleEntry,
		effect::EffectDB const& effectDB,
		effect::drawstyle::Bitmask& drawStyleBitmaskOut,
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

			drawStyleBitmaskOut |= effect::drawstyle::GetDrawStyleBitmaskByName(ruleName, modeName);
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
		std::string const& effectName = effectBlock.at(key_name).template get<std::string>();

		// "Name": Create an Effect
		effect::Effect newEffect(effectName.c_str());

		// "DefaultTechnique":
		if (effectBlock.contains(key_defaultTechnique))
		{
			std::string const& defaultTechniqueName = effectBlock.at(key_defaultTechnique).template get<std::string>();
			
			const TechniqueID defaultTechniqueID = effect::Technique::ComputeTechniqueID(defaultTechniqueName);

			SEAssert(!excludedTechniques.contains(defaultTechniqueID), "Default Technique cannot be excluded");

			newEffect.AddTechnique(
				effect::drawstyle::DefaultTechnique, effectDB.GetTechnique(defaultTechniqueID));
		}

		// "DrawStyles":
		if (effectBlock.contains(key_drawStyles) && !effectBlock.at(key_drawStyles).empty())
		{
			for (auto const& drawStyleEntry : effectBlock.at(key_drawStyles))
			{
				if (ExcludesPlatform(drawStyleEntry))
				{
					continue;
				}

				effect::drawstyle::Bitmask drawStyleBitmask = 0;
				effect::Technique const* technique = nullptr;

				ParseDrawStyleConditionEntry(drawStyleEntry, effectDB, drawStyleBitmask, technique);
				SEAssert(drawStyleBitmask != 0, "DrawStyle bitmask is zero. This is unexpected");
				SEAssert(technique != nullptr, "Technique pointer is null. This is unexpected");

				if (!excludedTechniques.contains(technique->GetTechniqueID()))
				{
					newEffect.AddTechnique(drawStyleBitmask, technique);
				}
			}
		}

		// "Buffers":
		if (effectBlock.contains(key_buffers) && !effectBlock.at(key_buffers).empty())
		{
			for (auto const& bufferName : effectBlock.at(key_buffers))
			{
				newEffect.AddBufferName(bufferName.template get<std::string>());
			}
		}

		return newEffect;
	}


	re::RasterizationState ParseRasterizationStateEntry(auto const& rasterizationStateEntry)
	{
		// Create a new RasterizationState, and update it as necessary:
		re::RasterizationState newRasterizationState;

		// "TopologyType":
		if (rasterizationStateEntry.contains(key_topologyType))
		{
			newRasterizationState.SetPrimitiveTopologyType(re::RasterizationState::CStrToPrimitiveTopologyType(
				rasterizationStateEntry.at(key_topologyType).template get<std::string>().c_str()));
		}

		// "RasterizerState":
		if (rasterizationStateEntry.contains(key_rasterizerState))
		{
			auto const& rasterizerBlock = rasterizationStateEntry.at(key_rasterizerState);

			// "FillMode":
			if (rasterizerBlock.contains(key_fillMode))
			{
				newRasterizationState.SetFillMode(re::RasterizationState::GetFillModeByName(
					rasterizerBlock.at(key_fillMode).template get<std::string>().c_str()));
			}

			// "FaceCullingMode":
			if (rasterizerBlock.contains(key_faceCullingMode))
			{
				newRasterizationState.SetFaceCullingMode(re::RasterizationState::GetFaceCullingModeByName(
					rasterizerBlock.at(key_faceCullingMode).template get<std::string>().c_str()));
			}

			// "WindingOrder":
			if (rasterizerBlock.contains(key_windingOrder))
			{
				newRasterizationState.SetWindingOrder(re::RasterizationState::GetWindingOrderByName(
					rasterizerBlock.at(key_windingOrder).template get<std::string>().c_str()));
			}

			// "DepthBias":
			if (rasterizerBlock.contains(key_depthBias))
			{
				newRasterizationState.SetDepthBias(rasterizerBlock.at(key_depthBias).template get<int>());
			}

			// "DepthBiasClamp":
			if (rasterizerBlock.contains(key_depthBiasClamp))
			{
				newRasterizationState.SetDepthBiasClamp(rasterizerBlock.at(key_depthBiasClamp).template get<float>());
			}

			// "SlopeScaledDepthBias":
			if (rasterizerBlock.contains(key_slopeScaledDepthBias))
			{
				newRasterizationState.SetSlopeScaledDepthBias(rasterizerBlock.at(key_slopeScaledDepthBias).template get<float>());
			}

			// "DepthClipEnable":
			if (rasterizerBlock.contains(key_depthClipEnable))
			{
				newRasterizationState.SetDepthClipEnabled(rasterizerBlock.at(key_depthClipEnable).template get<bool>());
			}

			// "MultisampleEnable":
			if (rasterizerBlock.contains(key_multisampleEnable))
			{
				newRasterizationState.SetMultiSampleEnabled(rasterizerBlock.at(key_multisampleEnable).template get<bool>());
			}

			// "AntialiasedLineEnable":
			if (rasterizerBlock.contains(key_antialiasedLineEnable))
			{
				newRasterizationState.SetAntiAliasedLineEnabled(rasterizerBlock.at(key_antialiasedLineEnable).template get<bool>());
			}

			// "ForcedSampleCount":
			if (rasterizerBlock.contains(key_forcedSampleCount))
			{
				newRasterizationState.SetForcedSampleCount(rasterizerBlock.at(key_forcedSampleCount).template get<uint8_t>());
			}

			// "ConservativeRaster":
			if (rasterizerBlock.contains(key_conservativeRaster))
			{
				newRasterizationState.SetConservativeRaster(rasterizerBlock.at(key_conservativeRaster).template get<bool>());
			}
		}

		if (rasterizationStateEntry.contains(key_depthStencilState))
		{
			auto const& depthStencilBlock = rasterizationStateEntry.at(key_depthStencilState);

			// "DepthTestEnabled":
			if (depthStencilBlock.contains(key_depthTestEnabled))
			{
				newRasterizationState.SetDepthTestEnabled(depthStencilBlock.at(key_depthTestEnabled).template get<bool>());
			}

			// "DepthWriteMask":
			if (depthStencilBlock.contains(key_depthWriteMask))
			{
				newRasterizationState.SetDepthWriteMask(re::RasterizationState::GetDepthWriteMaskByName(
					depthStencilBlock.at(key_depthWriteMask).template get<std::string>().c_str()));
			}

			// "DepthComparison":
			if (depthStencilBlock.contains(key_depthComparison))
			{
				newRasterizationState.SetDepthComparison(re::RasterizationState::GetComparisonByName(
					depthStencilBlock.at(key_depthComparison).template get<std::string>().c_str()));
			}

			// "StencilEnabled":
			if (depthStencilBlock.contains(key_stencilEnabled))
			{
				newRasterizationState.SetStencilEnabled(depthStencilBlock.at(key_stencilEnabled).template get<bool>());
			}

			// "StencilReadMask":
			if (depthStencilBlock.contains(key_stencilReadMask))
			{
				newRasterizationState.SetStencilReadMask(depthStencilBlock.at(key_stencilReadMask).template get<uint8_t>());
			}

			// "StencilWriteMask":
			if (depthStencilBlock.contains(key_stencilWriteMask))
			{
				newRasterizationState.SetStencilWriteMask(depthStencilBlock.at(key_stencilWriteMask).template get<uint8_t>());
			}

			auto ParseStencilOpDesc = [](auto const& stencilOpDesc) -> re::RasterizationState::StencilOpDesc
				{
					re::RasterizationState::StencilOpDesc desc{};

					if (stencilOpDesc.contains(key_stencilFailOp))
					{
						desc.m_failOp = re::RasterizationState::GetStencilOpByName(
							stencilOpDesc.at(key_stencilFailOp).template get<std::string>().c_str());
					}

					if (stencilOpDesc.contains(key_stencilDepthFailOp))
					{
						desc.m_depthFailOp = re::RasterizationState::GetStencilOpByName(
							stencilOpDesc.at(key_stencilDepthFailOp).template get<std::string>().c_str());
					}

					if (stencilOpDesc.contains(key_stencilPassOp))
					{
						desc.m_passOp = re::RasterizationState::GetStencilOpByName(
							stencilOpDesc.at(key_stencilPassOp).template get<std::string>().c_str());
					}

					if (stencilOpDesc.contains(key_stencilComparison))
					{
						desc.m_comparison = re::RasterizationState::GetComparisonByName(
							stencilOpDesc.at(key_stencilComparison).template get<std::string>().c_str());
					}

					return desc;
				};

			if (depthStencilBlock.contains(key_frontStencilOpDesc))
			{
				auto const& frontStencilOpDesc = rasterizationStateEntry.at(key_frontStencilOpDesc);
				re::RasterizationState::StencilOpDesc desc = ParseStencilOpDesc(frontStencilOpDesc);
				newRasterizationState.SetFrontFaceStencilOpDesc(desc);
			}

			if (depthStencilBlock.contains(key_backStencilOpDesc))
			{
				auto const& backStencilOpDesc = rasterizationStateEntry.at(key_backStencilOpDesc);
				re::RasterizationState::StencilOpDesc desc = ParseStencilOpDesc(backStencilOpDesc);
				newRasterizationState.SetBackFaceStencilOpDesc(desc);
			}
		}

		if (rasterizationStateEntry.contains(key_blendState))
		{
			auto const& blendStateBlock = rasterizationStateEntry.at(key_blendState);

			// "AlphaToCoverageEnable":
			if (blendStateBlock.contains(key_alphaToCoverageEnable))
			{
				newRasterizationState.SetAlphaToCoverageEnabled(
					blendStateBlock.at(key_alphaToCoverageEnable).template get<bool>());
			}

			// "IndependentBlendEnable":
			if (blendStateBlock.contains(key_independentBlendEnable))
			{
				newRasterizationState.SetIndependentBlendEnabled(
					blendStateBlock.at(key_independentBlendEnable).template get<bool>());
			}

			// "RenderTargets":
			if (blendStateBlock.contains(key_renderTargets))
			{
				uint8_t index = 0;
				for (auto const& renderTargetDesc : blendStateBlock.at(key_renderTargets))
				{
					re::RasterizationState::RenderTargetBlendDesc blendDesc{};

					// "BlendEnable":
					if (renderTargetDesc.contains(key_blendEnable))
					{
						blendDesc.m_blendEnable = renderTargetDesc.at(key_blendEnable).template get<bool>();
					}

					// "LogicOpEnable":
					if (renderTargetDesc.contains(key_logicOpEnable))
					{
						blendDesc.m_logicOpEnable = renderTargetDesc.at(key_logicOpEnable).template get<bool>();
					}

					// "SrcBlend":
					if (renderTargetDesc.contains(key_srcBlend))
					{
						blendDesc.m_srcBlend = re::RasterizationState::GetBlendModeByName(
							renderTargetDesc.at(key_srcBlend).template get<std::string>().c_str());
					}

					// "DstBlend":
					if (renderTargetDesc.contains(key_dstBlend))
					{
						blendDesc.m_dstBlend = re::RasterizationState::GetBlendModeByName(
							renderTargetDesc.at(key_dstBlend).template get<std::string>().c_str());
					}

					// "BlendOp":
					if (renderTargetDesc.contains(key_blendOp))
					{
						blendDesc.m_blendOp = re::RasterizationState::GetBlendOpByName(
							renderTargetDesc.at(key_blendOp).template get<std::string>().c_str());
					}

					// "SrcBlendAlpha":
					if (renderTargetDesc.contains(key_srcBlendAlpha))
					{
						blendDesc.m_srcBlendAlpha = re::RasterizationState::GetBlendModeByName(
							renderTargetDesc.at(key_srcBlendAlpha).template get<std::string>().c_str());
					}

					// "DstBlendAlpha":
					if (renderTargetDesc.contains(key_dstBlendAlpha))
					{
						blendDesc.m_dstBlendAlpha = re::RasterizationState::GetBlendModeByName(
							renderTargetDesc.at(key_dstBlendAlpha).template get<std::string>().c_str());
					}

					// "BlendOpAlpha":
					if (renderTargetDesc.contains(key_blendOpAlpha))
					{
						blendDesc.m_blendOpAlpha = re::RasterizationState::GetBlendOpByName(
							renderTargetDesc.at(key_blendOpAlpha).template get<std::string>().c_str());
					}

					// "LogicOp":
					if (renderTargetDesc.contains(key_logicOp))
					{
						blendDesc.m_logicOp = re::RasterizationState::GetLogicOpByName(
							renderTargetDesc.at(key_logicOp).template get<std::string>().c_str());
					}

					// "RenderTargetWriteMask":
					if (renderTargetDesc.contains(key_renderTargetWriteMask))
					{
						blendDesc.m_renderTargetWriteMask = 
							renderTargetDesc.at(key_renderTargetWriteMask).template get<uint8_t>();
					}

					newRasterizationState.SetRenderTargetBlendDesc(blendDesc, index);
					++index;
				}
			}
		}

		return newRasterizationState;
	}


	effect::Technique ParseJSONTechniqueEntry(
		auto const& techniqueEntry,
		effect::EffectDB const& effectDB)
	{
		SEAssert(techniqueEntry.contains(key_name), "Incomplete Technique definition");

		// "Name":
		std::string const& techniqueName = techniqueEntry.at(key_name).template get<std::string>();

		// "*Shader" names:
		std::vector<re::Shader::Metadata> shaderMetadata;
		re::Shader::ShaderType firstShaderType = re::Shader::ShaderType::ShaderType_Count;
		for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
		{
			if (techniqueEntry.contains(keys_shaderTypes[shaderTypeIdx]))
			{
				if (firstShaderType == re::Shader::ShaderType::ShaderType_Count)
				{
					firstShaderType = static_cast<re::Shader::ShaderType>(shaderTypeIdx);
				}
				SEAssert(re::Shader::IsSamePipelineType(
					firstShaderType, static_cast<re::Shader::ShaderType>(shaderTypeIdx)),
					"Technique can only define shaders of the same pipeline type");

				shaderMetadata.emplace_back(re::Shader::Metadata{
					techniqueEntry.at(keys_shaderTypes[shaderTypeIdx]).template get<std::string>(),
					techniqueEntry.at(keys_entryPointNames[shaderTypeIdx]).template get<std::string>(),
					static_cast<re::Shader::ShaderType>(shaderTypeIdx) });
			}
		}

		SEAssert(!re::Shader::IsRasterizationType(firstShaderType) || techniqueEntry.contains(key_rasterizationState),
			"Failed to find RasterizationState entry. This is required for rasterization pipeline shaders");

		SEAssert(!re::Shader::IsRasterizationType(firstShaderType) || techniqueEntry.contains(key_vertexStream),
			"Failed to find VertexStream entry. This is required for rasterization pipeline shaders");

		re::RasterizationState const* rasterizationState = nullptr;
		re::VertexStreamMap const* vertexStreamMap = nullptr;
		if (re::Shader::IsRasterizationType(firstShaderType))
		{
			std::string const& rasterizationStateName = techniqueEntry.at(key_rasterizationState).template get<std::string>();
			rasterizationState = effectDB.GetRasterizationState(rasterizationStateName);

			std::string const& vertexStreamName = techniqueEntry.at(key_vertexStream).template get<std::string>();
			vertexStreamMap = effectDB.GetVertexStreamMap(vertexStreamName);
		}

		return effect::Technique(techniqueName.c_str(), std::move(shaderMetadata), rasterizationState, vertexStreamMap);
	}


	inline gr::VertexStream::Type SemanticNameToStreamType(std::string const& semanticName)
	{
		static const std::unordered_map<util::CHashKey, gr::VertexStream::Type> s_semanticLowerToStreamType =
		{
			{ util::CHashKey("position"),		gr::VertexStream::Type::Position },
			{ util::CHashKey("sv_position"),	gr::VertexStream::Type::Position },
			{ util::CHashKey("normal"),			gr::VertexStream::Type::Normal },
			//{ util::CHashKey("binormal"),		gr::VertexStream::Type::Binormal },
			{ util::CHashKey("tangent"),		gr::VertexStream::Type::Tangent },
			{ util::CHashKey("texcoord"),		gr::VertexStream::Type::TexCoord },
			{ util::CHashKey("color"),			gr::VertexStream::Type::Color },
			{ util::CHashKey("blendindices"),	gr::VertexStream::Type::BlendIndices },
			{ util::CHashKey("blendweight"),	gr::VertexStream::Type::BlendWeight },
			/*{ util::CHashKey("pointsize"),	gr::VertexStream::Type::PointSize },*/
			{ util::CHashKey("index"),			gr::VertexStream::Type::Index },
		};
		SEStaticAssert(gr::VertexStream::Type_Count == 8, "Number of vertex stream types changed. This must be updated");

		const util::CHashKey semanticNameLowerHashkey = util::CHashKey::Create(util::ToLower(semanticName));

		SEAssert(s_semanticLowerToStreamType.contains(semanticNameLowerHashkey), "Invalid semantic name");

		return s_semanticLowerToStreamType.at(semanticNameLowerHashkey);
	}


	re::VertexStreamMap ParseVertexStreamDesc(auto const& vertexStreamsEntry)
	{
		re::VertexStreamMap vertexStreamMap;

		uint8_t slotIndex = 0; // Monotonically-increasing

		for (auto const& slotDesc : vertexStreamsEntry.at(key_slots))
		{
			std::string const& dataType = slotDesc.at(key_dataType).template get<std::string>();
			std::string const& name = slotDesc.at(key_name).template get<std::string>();
			std::string const& semantic = slotDesc.at(key_semantic).template get<std::string>();

			auto ExtractSemanticNameAndIndex = [](std::string& semanticName, uint8_t& semanticIdx)
				{
					constexpr char const* k_digits = "0123456789";
					const size_t semanticNumberIdx = semanticName.find_first_of(k_digits);

					// If the semantic contains an index, seperate them. Otherwise, do nothing.
					if (semanticNumberIdx != std::string::npos)
					{
						semanticIdx = std::stoi(semanticName.substr(semanticNumberIdx, semanticName.size()));
						semanticName = semanticName.substr(0, semanticNumberIdx);
					}
				};

			std::string semanticName = semantic;
			uint8_t semanticIdx = 0; // Assume 0 if no semantic index is specified (e.g. NORMAL, SV_Position, etc)
			ExtractSemanticNameAndIndex(semanticName, semanticIdx);

			const gr::VertexStream::Type streamType = SemanticNameToStreamType(semanticName);
			const re::DataType streamDataType = re::StrToDataType(dataType);

			vertexStreamMap.SetSlotIdx(streamType, semanticIdx, streamDataType, slotIndex++);
		}

		return vertexStreamMap;
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
			std::scoped_lock lock(m_effectsMutex, m_techniquesMutex, m_rasterizationStatesMutex, m_vertexStreamMapsMutex);

			m_effects.clear();
			m_techniques.clear();
			m_rasterizationStates.clear();
			m_vertexStreamMaps.clear();
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

			// Enqueue Effect loading as a job (Effects trigger Shader parsing/loading)
			std::vector<std::future<void>> taskFutures;
			taskFutures.reserve(effectManifestJSON.at(key_effectsBlock).size());

			static const bool s_threadedEffectLoading = 
				core::Config::Get()->KeyExists(core::configkeys::k_singleThreadEffectLoading) == false;

			for (auto const& effectManifestEntry : effectManifestJSON.at(key_effectsBlock))
			{
				if (s_threadedEffectLoading)
				{
					taskFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[&effectManifestEntry, this]()
						{
							std::string const& effectDefinitionFilename = effectManifestEntry.template get<std::string>();
							try
							{
								LoadEffect(effectDefinitionFilename);
							}
							catch (nlohmann::json::parse_error parseException)
							{
								std::string const& error = std::format(
									"Failed to parse the Effect definition file \"{}\"\n{}",
									effectDefinitionFilename,
									parseException.what());
								SEAssertF(error.c_str());
							}
						}));
				}
				else
				{
					std::string const& effectDefinitionFilename = effectManifestEntry.template get<std::string>();
					LoadEffect(effectDefinitionFilename);
				}
			}

			// Wait for loading to complete:
			if (s_threadedEffectLoading)
			{
				for (auto const& taskFuture : taskFutures)
				{
					taskFuture.wait();
				}
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

		LOG("Effect loading complete!");
	}


	effect::Effect const* EffectDB::LoadEffect(std::string const& effectName)
	{
		const EffectID effectID = effect::Effect::ComputeEffectID(effectName);
		if (HasEffect(effectID)) // Only process new Effects
		{
			return GetEffect(effectID);
		}

		constexpr char const* k_effectDefinitionFileExtension = ".json";
		std::string const& effectFilepath = 
			std::format("{}{}{}", core::configkeys::k_effectDirName, effectName, k_effectDefinitionFileExtension);

		LOG("Loading Effect \"%s\"...", effectFilepath.c_str());

		std::ifstream effectInputStream(effectFilepath);
		SEAssert(effectInputStream.is_open(), "Failed to open Effect definition input stream");

		const bool allowExceptions = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonAllowExceptionsKey);
		const bool ignoreComments = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonIgnoreCommentsKey);

		nlohmann::json effectJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			effectJSON = nlohmann::json::parse(effectInputStream, parserCallback, allowExceptions, ignoreComments);

			std::unordered_set<TechniqueID> excludedTechniques;

			// "RasterizationStates":
			if (effectJSON.contains(key_RasterizationStatesBlock) && !effectJSON.at(key_RasterizationStatesBlock).empty())
			{
				auto const& rasterizationStateBlock = effectJSON.at(key_RasterizationStatesBlock);
				for (auto const& piplineStateEntry : rasterizationStateBlock)
				{
					SEAssert(piplineStateEntry.contains(key_name), "Incomplete RasterizationState definition");

					if (ExcludesPlatform(piplineStateEntry))
					{
						continue;
					}

					std::string const& rasterizationStateName = piplineStateEntry.at(key_name).template get<std::string>();
					AddRasterizationState(rasterizationStateName, ParseRasterizationStateEntry(piplineStateEntry));
				}
			}

			//"VertexStreams":
			if (effectJSON.contains(key_vertexStreams) && !effectJSON.at(key_vertexStreams).empty())
			{
				for (auto const& vertexStreamEntry : effectJSON.at(key_vertexStreams))
				{
					std::string const& vertexStreamDescName = vertexStreamEntry.at(key_name).template get<std::string>();

					if (!HasVertexStreamMap(vertexStreamDescName))
					{
						AddVertexStreamMap(vertexStreamDescName, ParseVertexStreamDesc(vertexStreamEntry));
					}
				}
			}

			if (effectJSON.contains(key_effectBlock))
			{
				auto const& effectBlock = effectJSON.at(key_effectBlock);

				SEAssert(effectBlock.contains(key_name) &&
					effectName == effectBlock.at(key_name).template get<std::string>(),
					"Effect name and effect definition filename do not match. This is unexpected");

				// "ExcludedPlatforms":
				if (ExcludesPlatform(effectBlock))
				{
					LOG("Effect \"%s\" is excluded on the current platform. Skipping.",
						effectFilepath.c_str());

					return nullptr;
				}
				else
				{
					// "Parents": Parsed first to ensure dependencies exist
					std::vector<std::pair<drawstyle::Bitmask, Technique const*>> allParentTechniques;
					if (effectBlock.contains(key_parents) &&
						!effectBlock.at(key_parents).empty())
					{
						for (auto const& parent : effectBlock.at(key_parents))
						{
							std::string const& parentName = parent.template get<std::string>();
							Effect const* parentEffect = LoadEffect(parentName);
							
							if (parentEffect) // It's valid for the parent Effect to be null (e.g. platform exclusions)
							{
								std::unordered_map<drawstyle::Bitmask, Technique const*> const& parentTechniques =
									parentEffect->GetAllTechniques();

								for (auto const& technique : parentTechniques)
								{
									allParentTechniques.emplace_back(technique);
								}
							}
						}
					}

					// "Techniques":
					if (effectBlock.contains(key_techniques) && !effectBlock.at(key_techniques).empty())
					{
						for (auto const& techniqueEntry : effectBlock.at(key_techniques))
						{
							std::string const& techniqueName = techniqueEntry.at(key_name).template get<std::string>();

							// "ExcludedPlatforms": Skip this technique if it is excluded
							if (ExcludesPlatform(techniqueEntry))
							{
								excludedTechniques.emplace(effect::Technique::ComputeTechniqueID(techniqueName));
								continue;
							}
							AddTechnique(ParseJSONTechniqueEntry(techniqueEntry, *this));
						}
					}

					// "Effect":
					Effect newEffect = ParseJSONEffectBlock(effectBlock, *this, excludedTechniques);


					// Post-processing:
					// ----------------
					
					// Add any inherited techniques:
					for (auto const& parentTechnique : allParentTechniques)
					{
						newEffect.AddTechnique(parentTechnique.first, parentTechnique.second);
					}

					// Finally, add the new Effect. We must do this last once the Effect is fully created
					return AddEffect(std::move(newEffect));
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

		return nullptr;
	}


	bool EffectDB::HasEffect(EffectID effectID) const
	{
		{
			std::shared_lock<std::shared_mutex> lock(m_effectsMutex);
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

			LOG(std::format("Added Effect \"{}\" with hash {}", 
				result.first->second.GetName(),
				result.first->second.GetNameHash()).c_str());
			
			return &(result.first->second);
		}
	}


	bool EffectDB::HasTechnique(TechniqueID techniqueID) const
	{
		{
			std::shared_lock<std::shared_mutex> lock(m_techniquesMutex);
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

			LOG("Added Technique \"%s\"", result.first->second.GetName().c_str());

			return &(result.first->second);
		}
	}


	bool EffectDB::HasRasterizationState(std::string const& name) const
	{
		{
			std::shared_lock<std::shared_mutex> lock(m_rasterizationStatesMutex);
			return m_rasterizationStates.contains(name);
		}
	}


	re::RasterizationState* EffectDB::AddRasterizationState(
		std::string const& name, re::RasterizationState&& newRasterizationState)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_rasterizationStatesMutex);

			if (m_rasterizationStates.contains(name))
			{
				SEAssert(m_rasterizationStates.at(name).GetDataHash() == newRasterizationState.GetDataHash(),
					"A RasterizationState with the given name but different data hash exists. Names must be unique");

				return &m_rasterizationStates.at(name);
			}

			auto result = m_rasterizationStates.emplace(name, std::move(newRasterizationState));

			return &(result.first->second);
		}
	}


	bool EffectDB::HasVertexStreamMap(std::string const& name) const
	{
		{
			std::shared_lock<std::shared_mutex> lock(m_vertexStreamMapsMutex);
			return m_vertexStreamMaps.contains(name);
		}
	}


	re::VertexStreamMap* EffectDB::AddVertexStreamMap(
		std::string const& name, re::VertexStreamMap const& vertexStreamMap)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_vertexStreamMapsMutex);

			if (m_vertexStreamMaps.contains(name))
			{
				SEAssert(m_vertexStreamMaps.at(name) == vertexStreamMap,
					"A VertexStreamMap with the given name but different configuration exists. VertexStreamMap names "
					"must be unique");

				return &m_vertexStreamMaps.at(name);
			}

			LOG("Added VertexStreamMap \"%s\"", name.c_str());

			return &m_vertexStreamMaps.emplace(name, vertexStreamMap).first->second;
		}
	}
}