#pragma once
#include "Assert.h"
#include "Config.h"
#include "Platform.h"
#include "RenderPipelineDesc.h"


namespace re
{
	void from_json(nlohmann::json const& jsonDesc, RenderPipelineDesc& pipelineDesc)
	{
		const platform::RenderingAPI api = en::Config::Get()->GetRenderingAPI();
		std::string currentPlatformVal;
		switch (api)
		{
		case platform::RenderingAPI::DX12: currentPlatformVal = RenderPipelineDesc::val_platformDX12; break;
		case platform::RenderingAPI::OpenGL: currentPlatformVal = RenderPipelineDesc::val_platformOpenGL; break;
		default: SEAssertF("Invalid RenderingAPI");
		}

		auto ExcludesPlatform = [&currentPlatformVal = std::as_const(currentPlatformVal)](auto entry) -> bool
			{
				if (entry.contains(RenderPipelineDesc::key_excludedPlatform))
				{
					for (auto const& excludedPlatform : entry[RenderPipelineDesc::key_excludedPlatform])
					{
						if (excludedPlatform.template get<std::string>() == currentPlatformVal)
						{
							return true;
						}
					}
				}
				return false;
			};

		pipelineDesc = {};

		pipelineDesc.m_pipelineName = jsonDesc[RenderPipelineDesc::key_pipelineName].template get<std::string>().c_str();

		// "RenderSystems":
		for (auto const& renderSystemEntry : jsonDesc[RenderPipelineDesc::key_renderSystemsBlock])
		{
			auto& currentRSDesc = pipelineDesc.m_renderSystems.emplace_back();

			// "RenderSystemName":
			currentRSDesc.m_renderSystemName =
				renderSystemEntry[RenderPipelineDesc::key_renderSystemName].template get<std::string>().c_str();

			// "Initialization": 
			auto const& initialization = renderSystemEntry[RenderPipelineDesc::key_initializationStepsBlock];
			for (size_t i = 0; i < initialization.size(); i++)
			{
				if (ExcludesPlatform(initialization.at(i)))
				{
					continue;
				}

				auto& newInitStep = currentRSDesc.m_initSteps.emplace_back();
				auto const& gsName =
					initialization.at(i)[RenderPipelineDesc::key_GSName].get_to(newInitStep.first);
				initialization.at(i)[RenderPipelineDesc::key_functionName].get_to(newInitStep.second);
				currentRSDesc.m_graphicsSystemNames.emplace(gsName);
			}

			// "Update":
			auto const& update = renderSystemEntry[RenderPipelineDesc::key_updateStepsBlock];
			for (size_t i = 0; i < update.size(); i++)
			{
				if (ExcludesPlatform(update.at(i)))
				{
					continue;
				}

				auto& newUpdateStep = currentRSDesc.m_updateSteps.emplace_back();

				auto const& gsName = update.at(i)[RenderPipelineDesc::key_GSName].get_to(newUpdateStep.first);
				update.at(i)[RenderPipelineDesc::key_functionName].get_to(newUpdateStep.second);
				currentRSDesc.m_graphicsSystemNames.emplace(gsName);
			}

			using GSName = re::RenderPipelineDesc::RenderSystemDescription::GSName;
			using SrcDstNamePairs = re::RenderPipelineDesc::RenderSystemDescription::SrcDstNamePairs;

			// "ResourceDependencies":
			// Note: A resource dependencies block isn't strictly necessary
			if (renderSystemEntry.contains(RenderPipelineDesc::key_resourceDependenciesBlock))
			{
				auto const& resourceDependencies = renderSystemEntry[RenderPipelineDesc::key_resourceDependenciesBlock];
				for (size_t dependencyIdx = 0; dependencyIdx < resourceDependencies.size(); dependencyIdx++)
				{
					auto const& currentGSEntry = resourceDependencies.at(dependencyIdx);

					// "GS":
					std::string const& currentGSName = 
						currentGSEntry[RenderPipelineDesc::key_GSName].template get<std::string>();


					// Helper: Parses lists of {"SourceName": "...", "DestinationName": "..."} entries
					auto ParseDependencyList = [&ExcludesPlatform](
						auto const& dependencyList,
						std::string const& dependencySourceGS,
						std::vector<std::pair<GSName, SrcDstNamePairs>>& curDependencies)
						{
							bool haveAddedFirstDependencyEntry = false;

							for (auto const& dependencyEntry : dependencyList)
							{
								if (ExcludesPlatform(dependencyEntry))
								{
									continue;
								}
								else if (!haveAddedFirstDependencyEntry)
								{
									// We must ensure we don't record empty GS dependencies for excluded platforms;
									// and we only want to add a single entry for the current source GS
									curDependencies.emplace_back(dependencySourceGS, SrcDstNamePairs());
									haveAddedFirstDependencyEntry = true;
								}

								SrcDstNamePairs& srcDstNames = curDependencies.back().second;

								srcDstNames.emplace_back(
									dependencyEntry[RenderPipelineDesc::key_srcName],
									dependencyEntry[RenderPipelineDesc::key_dstName]);
							}
						};


					// "Inputs":
					if (currentGSEntry.contains(RenderPipelineDesc::key_inputsList) &&
						!currentGSEntry[RenderPipelineDesc::key_inputsList].empty())
					{
						auto const& inputsList = currentGSEntry[RenderPipelineDesc::key_inputsList];
						for (auto const& inputEntry : inputsList)
						{
							// "GS":
							std::string const& dependencySourceGS =
								inputEntry[RenderPipelineDesc::key_GSName].template get<std::string>();

							SEAssert(dependencySourceGS != currentGSName, "A GS has listed itself as an input source");

							// "TextureDependencies":
							if (inputEntry.contains(RenderPipelineDesc::key_textureDependenciesList) &&
								!inputEntry[RenderPipelineDesc::key_textureDependenciesList].empty())
							{
								ParseDependencyList(
									inputEntry[RenderPipelineDesc::key_textureDependenciesList],
									dependencySourceGS, 
									currentRSDesc.m_textureInputs[currentGSName]);
							}

							// "DataDependencies":
							if (inputEntry.contains(RenderPipelineDesc::key_dataDependenciesList) &&
								!inputEntry[RenderPipelineDesc::key_dataDependenciesList].empty())
							{
								ParseDependencyList(
									inputEntry[RenderPipelineDesc::key_dataDependenciesList],
									dependencySourceGS,
									currentRSDesc.m_dataInputs[currentGSName]);
							}
						}
					}
				}
			}
		}
	}


	RenderPipelineDesc LoadRenderPipelineDescription(char const* scriptPath)
	{
		SEAssert(scriptPath, "Script path cannot be null");

		std::ifstream pipelineInputStream(scriptPath);
		SEAssert(pipelineInputStream.is_open(), "Failed to open render pipeline input stream");

		RenderPipelineDesc pipelineDesc;
		nlohmann::json pipelineDescJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			constexpr bool  k_allowExceptions = true;
			constexpr bool k_ignoreComments = true; // Allow C-style comments, which are NOT part of the JSON specs
			pipelineDescJSON =
				nlohmann::json::parse(pipelineInputStream, parserCallback, k_allowExceptions, k_ignoreComments);

			pipelineDesc = pipelineDescJSON.template get<RenderPipelineDesc>();
		}
		catch (nlohmann::json::parse_error parseException)
		{
			std::string const& error = std::format(
				"LoadRenderPipelineDescription failed to parse the render pipeline description file \"{}\"\n{}",
				scriptPath,
				parseException.what());
			SEAssertF(error.c_str());
		}

		return pipelineDesc;
	}
}