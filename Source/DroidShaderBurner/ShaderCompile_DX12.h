// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "EffectParsing.h"

#include "Renderer/Shader.h"


namespace droid
{
	struct HLSLCompileOptions
	{
		bool m_disableOptimizations = false;
		bool m_enableDebuggingInfo = false;
		bool m_allResourcesBound = false; // -all-resources-bound TODO: Support/test this
		bool m_treatWarningsAsErrors = false;
		bool m_enable16BitTypes = false; // TODO: Support/test this

		std::string m_targetProfile = "6_6"; // -T <profile>, for <profile>: ps_6_6/vs_6_6/gs_6_6/hs_6_6/ds_6_6/cs_6_6/etc

		uint8_t m_optimizationLevel = 3; // 3 = default, {O0...O3}

		bool m_multithreadedCompilation = true;
	};

	droid::ErrorCode CompileShader_HLSL(
		std::string const& directXCompilerExePath,
		PROCESS_INFORMATION&,
		HLSLCompileOptions const&,
		std::vector<std::string> const& includeDirectories,
		std::string const& extensionlessSrcFilename,
		std::string const& entryPointName,
		re::Shader::ShaderType,
		std::vector<std::string> const& defines,
		std::string const& outputDir);

	droid::ErrorCode PrintHLSLCompilerVersion(std::string const& directXCompilerExePath);
}