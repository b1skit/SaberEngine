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
		static constexpr char const* key_declarationsBlock = "Declarations";
		static constexpr char const* key_inputsList = "Inputs";
		static constexpr char const* key_textureDependenciesList = "TextureDependencies";
		static constexpr char const* key_dataDependenciesList = "DataDependencies";
		static constexpr char const* key_srcName = "SourceName";
		static constexpr char const* key_dstName = "DestinationName";
		static constexpr char const* key_GSName = "GraphicsSystem";
		static constexpr char const* key_excludedPlatform = "ExcludedPlatforms";
		
		static constexpr char const* val_platformDX12 = "DX12";
		static constexpr char const* val_platformOpenGL = "OpenGL";

		std::string m_pipelineName;

		struct RenderSystemDescription
		{
			std::string m_renderSystemName;
			std::unordered_set<std::string> m_graphicsSystemNames; // Unique set of non-excluded GS names
			std::vector<std::string> m_pipelineOrder; // GS names: "Declarations" ordering == pipeline construction order
			
			// Map each GS to a list of {GS, texture name} dependencies:
			using GSName = std::string;
			using SrcDstNamePairs = std::vector<std::pair<std::string, std::string>>;
			std::unordered_map<GSName, std::vector<std::pair<GSName, SrcDstNamePairs>>> m_textureInputs;

			std::unordered_map<GSName, std::vector<std::pair<GSName, SrcDstNamePairs>>> m_dataInputs;
		};
		std::vector<RenderSystemDescription> m_renderSystems;
	};


	void from_json(nlohmann::json const& jsonDesc, RenderPipelineDesc& pipelineDesc);


	RenderPipelineDesc LoadRenderPipelineDescription(char const* scriptPath);
}