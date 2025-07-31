#pragma once
#include "EnumTypes.h"
#include "RenderPipelineDesc.h"

#include "Core/Assert.h"
#include "Core/Config.h"

using GSName = gr::RenderPipelineDesc::GSName;
using SrcDstNamePairs = gr::RenderPipelineDesc::SrcDstNamePairs;


namespace gr
{
	void from_json(nlohmann::json const& jsonDesc, RenderPipelineDesc& renderSysDesc)
	{
		std::string const& currentPlatformStr = platform::RenderingAPIToCStr(
			core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey));

		auto ExcludesPlatform = [&currentPlatformVal = std::as_const(currentPlatformStr)](auto entry) -> bool
			{
				// "ExcludedPlatforms":
				if (entry.contains(RenderPipelineDesc::key_excludedPlatforms))
				{
					for (auto const& excludedPlatform : entry[RenderPipelineDesc::key_excludedPlatforms])
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
			if (jsonDesc.contains(RenderPipelineDesc::key_pipelineMetadataBlock) &&
				!jsonDesc[RenderPipelineDesc::key_pipelineMetadataBlock].empty())
			{
				auto const& pipelineMetadata = jsonDesc.at(RenderPipelineDesc::key_pipelineMetadataBlock);

				if (ExcludesPlatform(pipelineMetadata))
				{
					SEAssertF("Error: Trying to load a pipeline that explicitely excludes the current rendering API");
				}

				// "Name":
				if (pipelineMetadata.contains(RenderPipelineDesc::key_pipelineName))
				{
					renderSysDesc.m_name =
						pipelineMetadata[RenderPipelineDesc::key_pipelineName].template get<std::string>();
				}

				// "RuntimeSettings":
				if (pipelineMetadata.contains(RenderPipelineDesc::key_runtimeSettingsList) &&
					!pipelineMetadata[RenderPipelineDesc::key_runtimeSettingsList].empty())
				{
					std::vector<std::pair<std::string, std::string>>& runtimeSettings =
						renderSysDesc.m_configRuntimeSettings;

					auto const& settingsList = pipelineMetadata[RenderPipelineDesc::key_runtimeSettingsList];
					for (auto const& settingEntry : settingsList)
					{
						SEAssert(settingEntry.contains(RenderPipelineDesc::key_settingName),
							"RuntimeSettings must contain a \"Settings\" entry");

						std::string settingName = settingEntry[RenderPipelineDesc::key_settingName].template get<std::string>();

						// It's valid for a setting to not have a value: We'll set it as a boolean true if so
						std::string settingValue;
						if (settingEntry.contains(RenderPipelineDesc::key_settingValue))
						{
							settingValue = settingEntry[RenderPipelineDesc::key_settingValue].template get<std::string>();
						}

						runtimeSettings.emplace_back(settingName, settingValue);
					}
				}
			}

			// "Pipeline": 
			auto const& pipelineBlock = jsonDesc[RenderPipelineDesc::key_pipelineBlock];
			for (auto const& pipelineEntry : pipelineBlock)
			{
				if (ExcludesPlatform(pipelineEntry))
				{
					continue;
				}

				auto& newPipelineStep = renderSysDesc.m_pipelineOrder.emplace_back();
				auto const& currentGSName = pipelineEntry[RenderPipelineDesc::key_GSName].get_to(newPipelineStep);
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
								dependencyEntry[RenderPipelineDesc::key_srcName],
								dependencyEntry[RenderPipelineDesc::key_dstName]);
						}
					};


				// "Inputs":
				if (pipelineEntry.contains(RenderPipelineDesc::key_inputsList) &&
					!pipelineEntry[RenderPipelineDesc::key_inputsList].empty())
				{
					auto const& inputsList = pipelineEntry[RenderPipelineDesc::key_inputsList];
					for (auto const& inputEntry : inputsList)
					{
						// "GS":
						std::string const& dependencySourceGSName =
							inputEntry[RenderPipelineDesc::key_GSName].template get<std::string>();

						SEAssert(dependencySourceGSName != currentGSName, "A GS has listed itself as an input source");

						// "TextureDependencies":
						if (inputEntry.contains(RenderPipelineDesc::key_textureDependenciesList) &&
							!inputEntry[RenderPipelineDesc::key_textureDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderPipelineDesc::key_textureDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_textureInputs[currentGSName]);
						}

						// "BufferDependencies":
						if (inputEntry.contains(RenderPipelineDesc::key_bufferDependenciesList) &&
							!inputEntry[RenderPipelineDesc::key_bufferDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderPipelineDesc::key_bufferDependenciesList],
								dependencySourceGSName,
								renderSysDesc.m_bufferInputs[currentGSName]);
						}

						// "DataDependencies":
						if (inputEntry.contains(RenderPipelineDesc::key_dataDependenciesList) &&
							!inputEntry[RenderPipelineDesc::key_dataDependenciesList].empty())
						{
							ParseDependencyList(
								inputEntry[RenderPipelineDesc::key_dataDependenciesList],
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


	RenderPipelineDesc LoadPipelineDescription(char const* filepath)
	{
		SEAssert(filepath, "File path cannot be null");

		LOG("Loading pipeline description from \"%s\"...", filepath);

		std::ifstream pipelineInputStream(filepath);
		SEAssert(pipelineInputStream.is_open(), "Failed to open render pipeline input stream");

		const bool allowExceptions = core::Config::GetValue<bool>(core::configkeys::k_jsonAllowExceptionsKey);
		const bool ignoreComments = core::Config::GetValue<bool>(core::configkeys::k_jsonIgnoreCommentsKey);

		RenderPipelineDesc systemDesc;
		nlohmann::json pipelineDescJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			pipelineDescJSON =
				nlohmann::json::parse(pipelineInputStream, parserCallback, allowExceptions, ignoreComments);

			systemDesc = pipelineDescJSON.template get<RenderPipelineDesc>();
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