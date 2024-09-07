// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "EffectParsing.h"

#include "Renderer/Shader.h"


namespace droid
{
	time_t GetMostRecentlyModifiedFileTime(std::string const& filesystemTarget);

	void CleanDirectory(std::string const& dirPath, bool recreateDir = true);


	uint64_t ComputeShaderVariantID(
		re::Shader::ShaderType, std::string const& entryPointName, std::vector<std::string> const& techniqueDefines);

	std::string BuildExtensionlessShaderVariantName(std::string const& extensionlessShaderName, uint64_t variantID);


	// Data types that will be packed into STL containers
	struct TechniqueDesc
	{
		std::string Name;
		std::array<std::string, re::Shader::ShaderType_Count> _Shader;
		std::array<std::string, re::Shader::ShaderType_Count> _ShaderEntryPoint;
		std::string PipelineState;
		std::string VertexStream;
		std::set<std::string> ExcludedPlatforms;
		std::vector<std::string> Defines;

		// Metadata: Used during Droid processing, not read to/from JSON
		std::array<uint64_t, re::Shader::ShaderType_Count> m_shaderVariantIDs;
	};
	void to_json(nlohmann::json& json, TechniqueDesc const& technique);
	void from_json(nlohmann::json const& json, TechniqueDesc& technique);
}
