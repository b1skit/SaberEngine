// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "ParseDB.h"

#include "Core/Util/FileIOUtils.h"


namespace droid
{
	bool DoParsingAndCodeGen(ParseParams const& parseParams)
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
			// No modification needed - return false for didModify
			return false;
		}

		ParseDB parseDB(parseParams);

		bool didModify = false;

		bool parseModified = parseDB.Parse();
		didModify = parseModified;
		
		if (parseParams.m_doCppCodeGen &&
			(!isSameBuildConfig ||
			!cppGenDirNewer))
		{
			bool cppGenModified = parseDB.GenerateCPPCode();
			didModify = didModify || cppGenModified;
		}

		if (parseParams.m_compileShaders &&
			(!isSameBuildConfig ||
				!hlslGenDirNewer ||
				!glslGenDirNewer ||
				!hlslOutputNewer ||
				!glslOutputNewer ||
				!commonSrcNewer))
		{
			bool shaderGenModified = parseDB.GenerateShaderCode();
			didModify = didModify || shaderGenModified;

			bool compileModified = parseDB.CompileShaders();
			didModify = didModify || compileModified;

			// Write the build configuration marker files:
			util::SetBuildConfigurationMarker(parseParams.m_hlslShaderOutputDir, parseParams.m_buildConfiguration);
			util::SetBuildConfigurationMarker(parseParams.m_glslShaderOutputDir, parseParams.m_buildConfiguration);
		}

		return didModify;
	}
}