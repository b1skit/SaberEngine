#pragma once
#include "Platform.h"
#include "RenderManager.h"
#include "RenderSystemDesc.h"

#include "Core/Assert.h"
#include "Core/Config.h"

using GSName = gr::RenderSystemDescription::GSName;
using SrcDstNamePairs = gr::RenderSystemDescription::SrcDstNamePairs;


namespace gr
{
	void from_json(nlohmann::json const& jsonDesc, RenderSystemDescription& renderSysDesc)
	{
		std::string const& currentPlatformStr = platform::RenderingAPIToCStr(re::RenderManager::Get()->GetRenderingAPI());

		auto ExcludesPlatform = [&currentPlatformVal = std::as_const(currentPlatformStr)](auto entry) -> bool
			{
				// "ExcludedPlatforms":
				if (entry.contains(RenderSystemDescription::key_excludedPlatforms))
				{
					for (auto const& excludedPlatform : entry[RenderSystemDescription::key_excludedPlatforms])
					{
						if (excludedPlatform.template get<std::string>() == currentPlatformVal)
						{
							return true;
						}
					}
				}
				return false;
			};

		renderSysDesc = {};

		try
		{
			// "PipelineMetadata":
			if (jsonDesc.contains(RenderSystemDescription::key_pipelineMetadataBlock) &&
				!jsonDesc[RenderSystemDescription::key_pipelineMetadataBlock].empty())
			{
				auto const& pipelineMetadata = jsonDesc.at(RenderSystemDescription::key_pipelineMetadataBlock);

				if (ExcludesPlatform(pipelineMetadata))
				{
					SEAssertF("Error: Trying to load a pipeline that explicitely excludes the current rendering API");
				}

				// "Name":
				if (pipelineMetadata.contains(RenderSystemDescription::key_pipelineName))
				{
					renderSysDesc.m_name = pipelineMetadata[RenderSystemDescription::key_pipelineName];
				}

				// "RequiredFeatures":
				if (pipelineMetadata.contains(RenderSystemDescription::key_requiredFeatures) &&
					!pipelineMetadata[RenderSystemDescription::key_requiredFeatures].empty())
				{
					renderSysDesc.m_requiredFeatures = pipelineMetadata[RenderSystemDescription::key_requiredFeatures];
				}
			}

			// "Pipeline": 
			auto const& pipelineBlock = jsonDesc[RenderSystemDescription::key_pipelineBlock];
			for (auto const& pipelineEntry : pipelineBlock)
			{
				if (ExcludesPlatform(pipelineEntry))
				{
					continue;
				}

				auto& newPipelineStep = renderSysDesc.m_pipelineOrder.emplace_back();
				auto const& currentGSName = pipelineEntry[RenderSystemDescription::key_GSName].get_to(newPipelineStep);
				renderSysDesc.m_graphicsSystemNames.emplace(currentGSName);


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
								dependencyEntry[RenderSystemDescription::key_srcName],
								dependencyEntry[RenderSystemDescription::key_dstName]);
						}
					};


				// "Inputs":
				if (pipelineEntry.contains(RenderSystemDescription::key_inputsList) &&
					!pipelineEntry[RenderSystemDescription::key_inputsList].empty())
				{
					auto const& inputsList = pipelineEntry[RenderSystemDescription::key_inputsList];
					for (auto const& inputEntry : inputsList)
					{
						// "GS":
						std::string const& dependencySourceGSName =
							inputEntry[RenderSystemDescription::key_GSName].template get<std::string>();

						SEAssert(dependencySourceGSName != currentGSName, "A GS has listed itself as an input source");

						// "TextureDependencies":
						if (inputEntry.contains(RenderSystemDescription::key_textureDependenciesList) &&
							!inputEntry[RenderSystemDescription::key_textureDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderSystemDescription::key_textureDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_textureInputs[currentGSName]);
						}

						// "BufferDependencies":
						if (inputEntry.contains(RenderSystemDescription::key_bufferDependenciesList) &&
							!inputEntry[RenderSystemDescription::key_bufferDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderSystemDescription::key_bufferDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_bufferInputs[currentGSName]);
						}

						// "DataDependencies":
						if (inputEntry.contains(RenderSystemDescription::key_dataDependenciesList) &&
							!inputEntry[RenderSystemDescription::key_dataDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderSystemDescription::key_dataDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_dataInputs[currentGSName]);
						}
					}
				}
			}
		}
		catch (std::exception const& e)
		{
			SEAssertF(e.what());
		}
	}


	RenderSystemDescription LoadPipelineDescription(char const* filepath)
	{
		SEAssert(filepath, "File path cannot be null");

		LOG("Loading pipeline description from \"%s\"...", filepath);

		std::ifstream pipelineInputStream(filepath);
		SEAssert(pipelineInputStream.is_open(), "Failed to open render pipeline input stream");

		const bool allowExceptions = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonAllowExceptionsKey);
		const bool ignoreComments = core::Config::Get()->GetValue<bool>(core::configkeys::k_jsonIgnoreCommentsKey);

		RenderSystemDescription systemDesc;
		nlohmann::json pipelineDescJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			pipelineDescJSON =
				nlohmann::json::parse(pipelineInputStream, parserCallback, allowExceptions, ignoreComments);

			systemDesc = pipelineDescJSON.template get<RenderSystemDescription>();
		}
		catch (nlohmann::json::parse_error parseException)
		{
			std::string const& error = std::format(
				"LoadPipelineDescription failed to parse the render pipeline description file \"{}\"\n{}",
				filepath,
				parseException.what());
			SEAssertF(error.c_str());
		}

		return systemDesc;
	}
}