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

		for (auto const& renderSystem : jsonDesc[RenderPipelineDesc::key_renderSystemsBlock])
		{
			auto& pipeline = pipelineDesc.m_renderSystems.emplace_back();

			pipeline.m_renderSystemName =
				renderSystem[RenderPipelineDesc::key_renderSystemName].template get<std::string>().c_str();

			auto const& initialization = renderSystem[RenderPipelineDesc::key_initializationSteps];
			for (size_t i = 0; i < initialization.size(); i++)
			{
				if (ExcludesPlatform(initialization.at(i)))
				{
					continue;
				}

				auto& newInitStep = pipeline.m_initSteps.emplace_back();
				auto const& gsName =
					initialization.at(i)[RenderPipelineDesc::key_GSName].get_to(newInitStep.first);
				initialization.at(i)[RenderPipelineDesc::key_functionName].get_to(newInitStep.second);
				pipeline.m_graphicsSystemNames.emplace(gsName);
			}

			auto const& update = renderSystem[RenderPipelineDesc::key_updateSteps];
			for (size_t i = 0; i < update.size(); i++)
			{
				if (ExcludesPlatform(update.at(i)))
				{
					continue;
				}

				auto& newUpdateStep = pipeline.m_updateSteps.emplace_back();

				auto const& gsName = update.at(i)[RenderPipelineDesc::key_GSName].get_to(newUpdateStep.first);
				update.at(i)[RenderPipelineDesc::key_functionName].get_to(newUpdateStep.second);
				pipeline.m_graphicsSystemNames.emplace(gsName);
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