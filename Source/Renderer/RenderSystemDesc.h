// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	struct RenderSystemDescription
	{
		// JSON keys/values:
		static constexpr char const* key_pipelineBlock = "Pipeline";
		static constexpr char const* key_inputsList = "Inputs";
		static constexpr char const* key_textureDependenciesList = "TextureDependencies";
		static constexpr char const* key_bufferDependenciesList = "BufferDependencies";
		static constexpr char const* key_dataDependenciesList = "DataDependencies";
		static constexpr char const* key_srcName = "SourceName";
		static constexpr char const* key_dstName = "DestinationName";
		static constexpr char const* key_GSName = "GraphicsSystem";
		static constexpr char const* key_excludedPlatforms = "ExcludedPlatforms";
		
		static constexpr char const* val_platformDX12 = "DX12";
		static constexpr char const* val_platformOpenGL = "OpenGL";


		// ---


		std::unordered_set<std::string> m_graphicsSystemNames; // Unique set of non-excluded GS names
		std::vector<std::string> m_pipelineOrder; // GS names: "Pipeline" block declaration order == construction order

		using GSName = std::string;
		using SrcDstNamePairs = std::vector<std::pair<std::string, std::string>>;

		// Map each GS to a list of {GS, dependency name}:
		std::unordered_map<GSName, std::vector<std::pair<GSName, SrcDstNamePairs>>> m_textureInputs;
		std::unordered_map<GSName, std::vector<std::pair<GSName, SrcDstNamePairs>>> m_bufferInputs;		
		std::unordered_map<GSName, std::vector<std::pair<GSName, SrcDstNamePairs>>> m_dataInputs;
	};


	void from_json(nlohmann::json const& jsonDesc, RenderSystemDescription& pipelineDesc);


	RenderSystemDescription LoadRenderSystemDescription(char const* scriptPath);
}