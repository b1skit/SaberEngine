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

					// "Inputs":
					if (currentGSEntry.contains(RenderPipelineDesc::key_inputsList) &&
						!currentGSEntry[RenderPipelineDesc::key_inputsList].empty())
					{
						using GSName = re::RenderPipelineDesc::RenderSystemDescription::GSName;
						using TexSrcDstNamePairs = re::RenderPipelineDesc::RenderSystemDescription::TexSrcDstNamePairs;

						std::vector<std::pair<GSName, TexSrcDstNamePairs>>& currentGSDependencies = 
							currentRSDesc.m_textureInputs.emplace(
								currentGSName, 
								std::vector<std::pair<GSName, TexSrcDstNamePairs>>()).first->second;

						auto const& inputsList = currentGSEntry[RenderPipelineDesc::key_inputsList];
						for (auto const& inputEntry : inputsList)
						{
							// "GS":
							std::string const& dependencySourceGS =
								inputEntry[RenderPipelineDesc::key_GSName].template get<std::string>();

							SEAssert(dependencySourceGS != currentGSName, "A GS has listed itself as an input source");

							// "TextureDependencies":
							bool haveAddedCurDependencyEntry = false;
							auto const& texDependenciesList = inputEntry[RenderPipelineDesc::key_textureDependenciesList];
							for (auto const& texDependencyEntry : texDependenciesList)
							{
								if (ExcludesPlatform(texDependencyEntry))
								{
									continue;
								}
								else if (!haveAddedCurDependencyEntry)
								{
									// Ensure we don't record empty GS dependencies for excluded platforms
									currentGSDependencies.emplace_back(dependencySourceGS, TexSrcDstNamePairs());
									haveAddedCurDependencyEntry = true;
								}

								TexSrcDstNamePairs& texSrcDstNames = currentGSDependencies.back().second;

								texSrcDstNames.emplace_back(
									texDependencyEntry[RenderPipelineDesc::key_srcName],
									texDependencyEntry[RenderPipelineDesc::key_dstName]
								);
							}							
						}
					}

					// "Accesses":
					if (currentGSEntry.contains(RenderPipelineDesc::key_accessesList) && 
						!currentGSEntry[RenderPipelineDesc::key_accessesList].empty())
					{
						std::unordered_set<std::string>& currentGSAccesses = currentRSDesc.m_accesses[currentGSName];

						auto const& accessesBlock = currentGSEntry[RenderPipelineDesc::key_accessesList];
						for (auto const& accessEntry : accessesBlock)
						{
							auto const& result = currentGSAccesses.emplace(accessEntry.template get<std::string>());
							SEAssert(*result.first != currentGSName, "A GS has listed itself in the accesses list");
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