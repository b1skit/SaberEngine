// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "ParseDB.h"

#include "Core/Definitions/EffectKeys.h"


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
		case ErrorCode::DataError: return "DataError";
		case ErrorCode::ConfigurationError: return "ConfigurationError";
		default: return "INVALID_ERROR_CODE";
		}
	}


	ErrorCode DoParsingAndCodeGen(ParseParams const& parseParams)
	{
		// Skip parsing if the generated code was modified more recently than the effect files:
		const time_t effectsDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_effectsDir);
		const time_t cppGenDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_cppCodeGenOutputDir);
		const time_t hlslGenDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_hlslCodeGenOutputDir);
		const time_t glslGenDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_glslCodeGenOutputDir);
		const time_t glslShaderOutputDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_glslShaderOutputDir);
		if (cppGenDirModificationTime > effectsDirModificationTime &&
			hlslGenDirModificationTime > effectsDirModificationTime &&
			glslGenDirModificationTime > effectsDirModificationTime &&
			glslShaderOutputDirModificationTime > effectsDirModificationTime)
		{
			return droid::ErrorCode::NoModification;
		}

		ParseDB parseDB(parseParams);

		droid::ErrorCode result = parseDB.Parse();
		if (result < 0)
		{
			return result;
		}

		result = parseDB.GenerateCPPCode();
		if (result < 0)
		{
			return result;
		}

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


	void CleanDirectory(std::string const& dirPath)
	{
		std::filesystem::remove_all(dirPath);
	}
}