// © 2024 Adam Badke. All rights reserved.
#include "ParseHelpers.h"

#include "Core/Util/HashUtils.h"
#include "Core/Util/TextUtils.h"

#include "Renderer/EffectKeys.h"


namespace droid
{
	time_t GetMostRecentlyModifiedFileTime(std::string const& filesystemTarget)
	{
		std::filesystem::path targetPath = filesystemTarget;

		// If the target doesn't exist, it hasn't ever been modified
		if (std::filesystem::exists(targetPath) == false)
		{
			return 0;
		}

		time_t oldestTime = 0;

		if (std::filesystem::is_directory(targetPath))
		{
			for (auto const& dirEntry : std::filesystem::recursive_directory_iterator(targetPath))
			{
				std::filesystem::file_time_type fileTime = std::filesystem::last_write_time(dirEntry);
				std::chrono::system_clock::time_point systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
				const time_t dirEntryWriteTime = std::chrono::system_clock::to_time_t(systemTime);
				oldestTime = std::max(oldestTime, dirEntryWriteTime);
			}
		}
		else
		{
			std::filesystem::file_time_type fileTime = std::filesystem::last_write_time(targetPath);
			std::chrono::system_clock::time_point systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
			const time_t targetWriteTime = std::chrono::system_clock::to_time_t(systemTime);
			oldestTime = std::max(oldestTime, targetWriteTime);
		}
		return oldestTime;
	}


	void CleanDirectory(std::string const& dirPath, bool recreateDir /*= true*/)
	{
		std::filesystem::remove_all(dirPath);
		if (recreateDir)
		{
			std::filesystem::create_directories(dirPath);
		}
	}


	uint64_t ComputeShaderVariantID(
		re::Shader::ShaderType shaderType,
		std::string const& entryPointName,
		std::vector<std::string> const& techniqueDefines)
	{
		if (entryPointName.empty())
		{
			return 0; // No entry point names means the shader type is not used
		}

		uint64_t variantID = 0;

		util::AddDataToHash(variantID, static_cast<uint64_t>(shaderType));

		util::CombineHash(variantID, util::HashString(entryPointName));

		for (auto const& define : techniqueDefines)
		{
			util::CombineHash(variantID, util::HashString(define));
		}

		return variantID;
	}


	std::string BuildExtensionlessShaderVariantName(std::string const& extensionlessShaderName, uint64_t variantID)
	{
		if (variantID == 0)
		{
			return extensionlessShaderName;
		}

		return std::format("{}_{}", extensionlessShaderName, variantID);
	}


	void TechniqueDesc::ComputeMetadata()
	{
		for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
		{
			m_shaderVariantIDs[shaderTypeIdx] = ComputeShaderVariantID(
				static_cast<re::Shader::ShaderType>(shaderTypeIdx),
				_ShaderEntryPoint[shaderTypeIdx],
				_Defines[shaderTypeIdx]);
		}
	}


	void TechniqueDesc::InheritFrom(TechniqueDesc const& parent)
	{
		// Simple inheritance: If the child has a property, it overrides the parent entirely.
		// Otherwise, copy anything the parent has that the child doesn't
		for (size_t i = 0; i < re::Shader::ShaderType_Count; ++i)
		{
			if (_Shader[i].empty())
			{
				_Shader[i] = parent._Shader[i];
			}
			if (_ShaderEntryPoint[i].empty())
			{
				_ShaderEntryPoint[i] = parent._ShaderEntryPoint[i];
			}
			if (_Defines[i].empty())
			{
				_Defines[i] = parent._Defines[i];
			}
		}

		if (RaterizationState.empty())
		{
			RaterizationState = parent.RaterizationState;
		}
		if (VertexStream.empty())
		{
			VertexStream = parent.VertexStream;
		}
		if (ExcludedPlatforms.empty())
		{
			ExcludedPlatforms = parent.ExcludedPlatforms;
		}

		// Metadata:
		ComputeMetadata();
	}


	void to_json(nlohmann::json& json, TechniqueDesc const& technique)
	{
		auto AddEntry = [&json](char const* key, std::string const& val)
			{
				if (!val.empty())
				{
					json[key] = val;
				}
			};

		AddEntry(key_name, technique.Name);

		for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
		{
			if (technique._Shader[shaderTypeIdx].empty())
			{
				continue;
			}

			AddEntry(keys_shaderTypes[shaderTypeIdx], 
				BuildExtensionlessShaderVariantName(
					technique._Shader[shaderTypeIdx], technique.m_shaderVariantIDs[shaderTypeIdx]));

			AddEntry(keys_entryPointNames[shaderTypeIdx], technique._ShaderEntryPoint[shaderTypeIdx]);
		}

		AddEntry(key_rasterizationState, technique.RaterizationState);
		AddEntry(key_vertexStream, technique.VertexStream);

		if (!technique.ExcludedPlatforms.empty())
		{
			json[key_excludedPlatforms] = technique.ExcludedPlatforms;
		}

		// Note: We exclude the "_Defines" block in the runtime version of the Effect definition
	}


	void from_json(nlohmann::json const& json, TechniqueDesc& technique)
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
			if (json.contains(keys_shaderDefines[shaderIdx]))
			{
				json.at(keys_shaderDefines[shaderIdx]).get_to(technique._Defines[shaderIdx]);
			}
		}

		if (json.contains(key_rasterizationState))
		{
			json.at(key_rasterizationState).get_to(technique.RaterizationState);
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

		// Metadata:
		technique.ComputeMetadata();
	}
}