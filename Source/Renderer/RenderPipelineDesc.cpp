#pragma once
#include "EnumTypes.h"
#include "RenderPipelineDesc.h"

#include "Core/Assert.h"
#include "Core/Config.h"

using GSName = gr::RenderPipelineDescription::GSName;
using SrcDstNamePairs = gr::RenderPipelineDescription::SrcDstNamePairs;


namespace gr
{
	void from_json(nlohmann::json const& jsonDesc, RenderPipelineDescription& renderSysDesc)
	{
		std::string const& currentPlatformStr = platform::RenderingAPIToCStr(
			core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey));

		auto ExcludesPlatform = [&currentPlatformVal = std::as_const(currentPlatformStr)](auto entry) -> bool
			{
				// "ExcludedPlatforms":
				if (entry.contains(RenderPipelineDescription::key_excludedPlatforms))
				{
					for (auto const& excludedPlatform : entry[RenderPipelineDescription::key_excludedPlatforms])
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
			if (jsonDesc.contains(RenderPipelineDescription::key_pipelineMetadataBlock) &&
				!jsonDesc[RenderPipelineDescription::key_pipelineMetadataBlock].empty())
			{
				auto const& pipelineMetadata = jsonDesc.at(RenderPipelineDescription::key_pipelineMetadataBlock);

				if (ExcludesPlatform(pipelineMetadata))
				{
					SEAssertF("Error: Trying to load a pipeline that explicitely excludes the current rendering API");
				}

				// "Name":
				if (pipelineMetadata.contains(RenderPipelineDescription::key_pipelineName))
				{
					renderSysDesc.m_name =
						pipelineMetadata[RenderPipelineDescription::key_pipelineName].template get<std::string>();
				}
			}

			// "Pipeline": 
			auto const& pipelineBlock = jsonDesc[RenderPipelineDescription::key_pipelineBlock];
			for (auto const& pipelineEntry : pipelineBlock)
			{
				if (ExcludesPlatform(pipelineEntry))
				{
					continue;
				}

				auto& newPipelineStep = renderSysDesc.m_pipelineOrder.emplace_back();
				auto const& currentGSName = pipelineEntry[RenderPipelineDescription::key_GSName].get_to(newPipelineStep);
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
								dependencyEntry[RenderPipelineDescription::key_srcName],
								dependencyEntry[RenderPipelineDescription::key_dstName]);
						}
					};


				// "Inputs":
				if (pipelineEntry.contains(RenderPipelineDescription::key_inputsList) &&
					!pipelineEntry[RenderPipelineDescription::key_inputsList].empty())
				{
					auto const& inputsList = pipelineEntry[RenderPipelineDescription::key_inputsList];
					for (auto const& inputEntry : inputsList)
					{
						// "GS":
						std::string const& dependencySourceGSName =
							inputEntry[RenderPipelineDescription::key_GSName].template get<std::string>();

						SEAssert(dependencySourceGSName != currentGSName, "A GS has listed itself as an input source");

						// "TextureDependencies":
						if (inputEntry.contains(RenderPipelineDescription::key_textureDependenciesList) &&
							!inputEntry[RenderPipelineDescription::key_textureDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderPipelineDescription::key_textureDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_textureInputs[currentGSName]);
						}

						// "BufferDependencies":
						if (inputEntry.contains(RenderPipelineDescription::key_bufferDependenciesList) &&
							!inputEntry[RenderPipelineDescription::key_bufferDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderPipelineDescription::key_bufferDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_bufferInputs[currentGSName]);
						}

						// "DataDependencies":
						if (inputEntry.contains(RenderPipelineDescription::key_dataDependenciesList) &&
							!inputEntry[RenderPipelineDescription::key_dataDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderPipelineDescription::key_dataDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_dataInputs[currentGSName]);
						}
					}
				}

				// "Flags":
				if (pipelineEntry.contains(RenderPipelineDescription::key_flagsList) &&
					!pipelineEntry[RenderPipelineDescription::key_flagsList].empty())
				{
					std::vector<std::pair<std::string, std::string>>& gsFlags = renderSysDesc.m_flags[currentGSName];

					auto const& flagsList = pipelineEntry[RenderPipelineDescription::key_flagsList];
					for (auto const& flagEntry : flagsList)
					{
						gsFlags.emplace_back(
							flagEntry[RenderPipelineDescription::key_flagName].template get<std::string>(),
							flagEntry[RenderPipelineDescription::key_flagValue].template get<std::string>());
					}
				}
			}
		}
		catch (std::exception const& e)
		{
			SEAssertF(e.what());
		}
	}


	RenderPipelineDescription LoadPipelineDescription(char const* filepath)
	{
		SEAssert(filepath, "File path cannot be null");

		LOG("Loading pipeline description from \"%s\"...", filepath);

		std::ifstream pipelineInputStream(filepath);
		SEAssert(pipelineInputStream.is_open(), "Failed to open render pipeline input stream");

		const bool allowExceptions = core::Config::GetValue<bool>(core::configkeys::k_jsonAllowExceptionsKey);
		const bool ignoreComments = core::Config::GetValue<bool>(core::configkeys::k_jsonIgnoreCommentsKey);

		RenderPipelineDescription systemDesc;
		nlohmann::json pipelineDescJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			pipelineDescJSON =
				nlohmann::json::parse(pipelineInputStream, parserCallback, allowExceptions, ignoreComments);

			systemDesc = pipelineDescJSON.template get<RenderPipelineDescription>();
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