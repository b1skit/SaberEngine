// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "ParseDB.h"

#include "Core/Util/FileIOUtils.h"
#include "Core/Util/TextUtils.h"

#include "Renderer/EffectKeys.h"


namespace droid
{
	constexpr char const* ErrorCodeToCStr(ErrorCode errorCode)
	{
		switch (errorCode)
		{
		case ErrorCode::Success: return "Success";
		case ErrorCode::NoModification: return "NoModification";
		case ErrorCode::FileError: return "FileError";
		case ErrorCode::JSONError: return "JSONError";
		case ErrorCode::ShaderError: return "ShaderError";
		case ErrorCode::GenerationError: return "GenerationError";
		case ErrorCode::ConfigurationError: return "ConfigurationError";
		case ErrorCode::DependencyError: return "DependencyError";
		default: return "INVALID_ERROR_CODE";
		}
	}


	ErrorCode DoParsingAndCodeGen(ParseParams const& parseParams)
	{
		const bool isSameBuildConfig =
			(util::GetBuildConfigurationMarker(parseParams.m_hlslShaderOutputDir) == parseParams.m_buildConfiguration) &&
			(util::GetBuildConfigurationMarker(parseParams.m_glslShaderOutputDir) == parseParams.m_buildConfiguration);

		// Skip parsing if the generated code was modified more recently than the effect files:
		const time_t effectDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_effectSourceDir);

		const bool cppGenDirNewer = 
			GetMostRecentlyModifiedFileTime(parseParams.m_cppCodeGenOutputDir) > effectDirModificationTime;

		const bool hlslGenDirNewer = 
			GetMostRecentlyModifiedFileTime(parseParams.m_hlslCodeGenOutputDir) > effectDirModificationTime;

		const bool glslGenDirNewer =
			GetMostRecentlyModifiedFileTime(parseParams.m_glslCodeGenOutputDir) > effectDirModificationTime;

		const time_t hlslSrcDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_hlslShaderSourceDir);
		const time_t hlslDstOutputTime = GetMostRecentlyModifiedFileTime(parseParams.m_hlslShaderOutputDir);
		const bool hlslOutputNewer =
			hlslDstOutputTime > effectDirModificationTime &&
			hlslDstOutputTime > hlslSrcDirModificationTime;

		const time_t glslSrcDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_glslShaderSourceDir);
		const time_t glslDstOutputTime = GetMostRecentlyModifiedFileTime(parseParams.m_glslShaderOutputDir);
		const bool glslOutputNewer =
			glslDstOutputTime > effectDirModificationTime &&
			glslDstOutputTime > glslSrcDirModificationTime;

		if (isSameBuildConfig &&
			cppGenDirNewer &&
			hlslGenDirNewer &&
			glslGenDirNewer &&
			hlslOutputNewer &&
			glslOutputNewer)
		{
			return droid::ErrorCode::NoModification;
		}

		ParseDB parseDB(parseParams);

		droid::ErrorCode result = droid::ErrorCode::Success;

		result = parseDB.Parse();
		if (result < 0)
		{
			return result;
		}

		if (!isSameBuildConfig ||
			!cppGenDirNewer)
		{
			result = parseDB.GenerateCPPCode();
			if (result < 0)
			{
				return result;
			}
		}

		if (!isSameBuildConfig ||
			!hlslGenDirNewer ||
			!glslGenDirNewer ||
			!hlslOutputNewer ||
			!glslOutputNewer)
		{
			result = parseDB.GenerateShaderCode();
			if (result < 0)
			{
				return result;
			}

			result = parseDB.CompileShaders();
			if (result < 0)
			{
				return result;
			}

			// Write the build configuration marker files:
			util::SetBuildConfigurationMarker(parseParams.m_hlslShaderOutputDir, parseParams.m_buildConfiguration);
			util::SetBuildConfigurationMarker(parseParams.m_glslShaderOutputDir, parseParams.m_buildConfiguration);
		}

		return result;
	}


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


	uint64_t ComputeShaderVariantID(std::vector<std::string> const& techniqueDefines)
	{
		uint64_t variantID = 0;

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
}