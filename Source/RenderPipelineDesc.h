// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	struct RenderPipelineDesc
	{
		// JSON keys/values:
		static constexpr char const* key_pipelineName = "PipelineName";
		static constexpr char const* key_renderSystemsBlock = "RenderSystems";
		static constexpr char const* key_renderSystemName = "RenderSystemName";
		static constexpr char const* key_initializationSteps = "Initialization";
		static constexpr char const* key_updateSteps = "Update";
		static constexpr char const* key_GSName = "GS";
		static constexpr char const* key_functionName = "Function";
		static constexpr char const* key_excludedPlatform = "ExcludedPlatforms";
		static constexpr char const* val_platformDX12 = "DX12";
		static constexpr char const* val_platformOpenGL = "OpenGL";

		std::string m_pipelineName;

		struct RenderSystemDescription
		{
			std::string m_renderSystemName;
			std::unordered_set<std::string> m_graphicsSystemNames; // Unique set of non-excluded GS names
			std::vector<std::pair<std::string, std::string>> m_initSteps; // {GS name, Function name}
			std::vector<std::pair<std::string, std::string>> m_updateSteps; // {GS name, Function name}
		};
		std::vector<RenderSystemDescription> m_renderSystems;
	};


	void from_json(nlohmann::json const& jsonDesc, RenderPipelineDesc& pipelineDesc);


	RenderPipelineDesc LoadRenderPipelineDescription(char const* scriptPath);
}