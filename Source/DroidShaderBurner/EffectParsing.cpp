// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "ParseDB.h"

#include "Core/Util/FileIOUtils.h"


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

		const time_t commonSrcDirModificationTime = GetMostRecentlyModifiedFileTime(parseParams.m_commonShaderSourceDir);
		const bool commonSrcNewer = 
			commonSrcDirModificationTime < hlslDstOutputTime &&
			commonSrcDirModificationTime < glslDstOutputTime;

		if (isSameBuildConfig &&
			cppGenDirNewer &&
			hlslGenDirNewer &&
			glslGenDirNewer &&
			hlslOutputNewer &&
			glslOutputNewer &&
			commonSrcNewer)
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
		
		if (parseParams.m_doCppCodeGen &&
			(!isSameBuildConfig ||
			!cppGenDirNewer))
		{
			result = parseDB.GenerateCPPCode();
			if (result < 0)
			{
				return result;
			}
		}

		if (parseParams.m_compileShaders &&
			(!isSameBuildConfig ||
				!hlslGenDirNewer ||
				!glslGenDirNewer ||
				!hlslOutputNewer ||
				!glslOutputNewer ||
				!commonSrcNewer))
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
}