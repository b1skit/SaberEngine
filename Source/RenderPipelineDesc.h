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
		static constexpr char const* key_initializationStepsBlock = "Initialization";
		static constexpr char const* key_updateStepsBlock = "Update";
		static constexpr char const* key_resourceDependenciesBlock = "ResourceDependencies";
		static constexpr char const* key_inputsList = "Inputs";
		static constexpr char const* key_accessesList = "Accesses";
		static constexpr char const* key_textureDependenciesList = "TextureDependencies";
		static constexpr char const* key_srcName = "SourceName";
		static constexpr char const* key_dstName = "DestinationName";
		static constexpr char const* key_GSName = "GraphicsSystem";
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
			
			// Map each GS to a list of {GS, texture name} dependencies:
			using GSName = std::string;
			using TexSrcDstNamePairs = std::vector<std::pair<std::string, std::string>>;
			std::unordered_map<GSName, std::vector<std::pair<GSName, TexSrcDstNamePairs>>> m_textureInputs;

			// Names of any GS's accessed during execution (e.g. via GraphicsSystemManager::GetGraphicsSystem<T>())
			std::unordered_map<std::string, std::unordered_set<std::string>> m_accesses;
		};
		std::vector<RenderSystemDescription> m_renderSystems;
	};


	void from_json(nlohmann::json const& jsonDesc, RenderPipelineDesc& pipelineDesc);


	RenderPipelineDesc LoadRenderPipelineDescription(char const* scriptPath);
}