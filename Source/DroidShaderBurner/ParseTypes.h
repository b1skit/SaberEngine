// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "EffectParsing.h"

#include "Core/Util/HashUtils.h"
#include "Core/Util/TextUtils.h"

#include "Renderer/EffectKeys.h"
#include "Renderer/Shader.h"


namespace droid
{
	// Datatypes that will be packed into STL containers
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
		uint64_t m_shaderVariantID = 0;
	};


	inline void to_json(nlohmann::json& json, TechniqueDesc const& technique)
	{
		auto AddEntry = [&json](char const* key, std::string const& val)
			{
				if (!val.empty())
				{
					json[key] = val;
				}
			};

		AddEntry(key_name, technique.Name);

		for (uint8_t shaderIdx = 0; shaderIdx < re::Shader::ShaderType_Count; ++shaderIdx)
		{
			if (technique._Shader[shaderIdx].empty())
			{
				continue;
			}

			AddEntry(keys_shaderTypes[shaderIdx], 
				BuildExtensionlessShaderVariantName(technique._Shader[shaderIdx], technique.m_shaderVariantID));

			AddEntry(keys_entryPointNames[shaderIdx], technique._ShaderEntryPoint[shaderIdx]);
		}

		AddEntry(key_pipelineState, technique.PipelineState);
		AddEntry(key_vertexStream, technique.VertexStream);

		json[key_excludedPlatforms] = technique.ExcludedPlatforms;
		
		// Note: We exclude the "Defines" block in the runtime version of the effect definition
	}


	inline void from_json(nlohmann::json const& json, TechniqueDesc& technique)
	{
		json.at(key_name).get_to(technique.Name);

		for (uint8_t shaderIdx = 0; shaderIdx < re::Shader::ShaderType_Count; ++shaderIdx)
		{
			if (json.contains(keys_shaderTypes[shaderIdx]))
			{
				json.at(keys_shaderTypes[shaderIdx]).get_to(technique._Shader[shaderIdx]);
			}
			if (json.contains(keys_entryPointNames[shaderIdx]))
			{
				json.at(keys_entryPointNames[shaderIdx]).get_to(technique._ShaderEntryPoint[shaderIdx]);
			}
		}

		if (json.contains(key_pipelineState))
		{
			json.at(key_pipelineState).get_to(technique.PipelineState);
		}
		if (json.contains(key_vertexStream))
		{
			json.at(key_vertexStream).get_to(technique.VertexStream);
		}
		if (json.contains(key_excludedPlatforms))
		{
			// Convert excluded platform names to lower case, later we'll check them against lowercase names
			std::set<std::string> excludedPlatforms;
			json.at(key_excludedPlatforms).get_to(excludedPlatforms);

			for (auto const& excluded : excludedPlatforms)
			{
				std::string const& existingToLower = util::ToLower(excluded);
				technique.ExcludedPlatforms.emplace(existingToLower);
			}
		}
		if (json.contains(key_defines))
		{
			json.at(key_defines).get_to(technique.Defines);
		}

		technique.m_shaderVariantID = ComputeShaderVariantID(technique.Defines);
	}
}
