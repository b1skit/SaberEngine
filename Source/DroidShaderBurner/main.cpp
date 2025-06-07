// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "ParseDB.h"
#include "TextStrings.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Util/FileIOUtils.h"


namespace
{
	// Note: Incoming command line args are transformed to lower case before comparison with these keys
	constexpr char const* k_delimiterChar = "-";
	constexpr char const* k_projectRootCmdLineArg = "-projectroot";
	
	constexpr char const* k_dx12ShaderCompilerCmdLineArg = "-dx12shadercompiler";
	constexpr char const* k_dx12TargetProfileArg = "-dx12targetprofile";

	constexpr char const* k_buildConfigCmdLineArg = "-buildconfig";
	constexpr char const* k_shadersOnlyCmdLineArg = "-shadersonly";
	constexpr char const* k_cleanCmdLineArg = "-clean";
	constexpr char const* k_cleanAndRebuildCmdLineArg = "-cleanandrebuild";
	
	constexpr char const* k_disallowJSONExceptionsCmdLineArg = "-disallowjsonexceptions";
	constexpr char const* k_disallowJSONCommentsCmdLineArg = "-disallowjsoncomments";
}


int main(int argc, char* argv[])
{
	std::cout << droid::k_logHeader;
	std::cout << "Launching...\n";

	droid::ErrorCode result = droid::ErrorCode::Success;

	droid::ParseParams parseParams{
		// Paths:
		.m_projectRootDir = "PROJECT_ROOT_DIRECTORY_NOT_SET", // Mandatory command line arg
		.m_runtimeAppDir = core::configkeys::k_appDirName,
		.m_effectSourceDir = "Source\\Renderer\\Effects\\",

		// Dependencies:
		.m_directXCompilerExePath = "DXC_COMPILER_EXE_PATH_NOT_SET", // Mandatory command line arg

		// Shader input paths:
		.m_hlslShaderSourceDir = "Source\\Renderer\\Shaders\\HLSL\\",
		.m_glslShaderSourceDir = "Source\\Renderer\\Shaders\\GLSL\\",
		.m_commonShaderSourceDir = "Source\\Renderer\\Shaders\\Common\\",
		.m_dependenciesDir = "Source\\Dependencies\\",

		// Output paths:
		.m_cppCodeGenOutputDir = "Source\\Generated\\",

		.m_hlslCodeGenOutputDir = "Source\\Renderer\\Shaders\\Generated\\HLSL\\",
		.m_hlslShaderOutputDir = 
			std::format("{}{}", core::configkeys::k_appDirName, core::configkeys::k_hlslShaderDirName),
		

		.m_glslCodeGenOutputDir = "Source\\Renderer\\Shaders\\Generated\\GLSL\\",
		.m_glslShaderOutputDir =
			std::format("{}{}", core::configkeys::k_appDirName, core::configkeys::k_glslShaderDirName),

		.m_runtimeEffectsDir = 
			std::format("{}{}", core::configkeys::k_appDirName, core::configkeys::k_effectDirName),

		// File names:
		.m_effectManifestFileName = core::configkeys::k_effectManifestFilename,

		.m_buildConfiguration = util::BuildConfiguration::Invalid, // Mandatory command line arg
	};

	// Handle command line args:
	bool doClean = false;
	bool doBuild = true;
	bool shadersOnly = false;
	bool projectRootDirReceived = false;
	bool dx12ShaderCompilerArgReceived = false;
	bool buildConfigArgReceived = false;
	if (argc > 0)
	{
		std::string commandLineArgs;
		for (int i = 1; i < argc; ++i)
		{
			auto AppendArg = [&commandLineArgs, i, argc](char const* currentArg)
				{
					commandLineArgs += currentArg;
					if (i + 1 < argc)
					{
						commandLineArgs += " ";
					}
				};
			AppendArg(argv[i]);

			std::string currentArg = argv[i];
			std::transform(
				currentArg.begin(),
				currentArg.end(),
				currentArg.begin(),
				[](unsigned char c) {return std::tolower(c); });

			if (currentArg == k_disallowJSONExceptionsCmdLineArg)
			{
				parseParams.m_allowJSONExceptions = false;
			}
			else if (currentArg == k_disallowJSONCommentsCmdLineArg)
			{
				parseParams.m_ignoreJSONComments = false;
			}
			else if (currentArg == k_cleanCmdLineArg)
			{
				doClean = true;
				doBuild = false;
			}
			else if (currentArg == k_cleanAndRebuildCmdLineArg)
			{
				doClean = true;
				doBuild = true;
			}
			else if (currentArg == k_shadersOnlyCmdLineArg)
			{
				shadersOnly = true;

				parseParams.m_doCppCodeGen = false;
				parseParams.m_compileShaders = true;
			}
			else if (currentArg == k_projectRootCmdLineArg)
			{
				if (i + 1 < argc && argv[i + 1][0] != *k_delimiterChar)
				{
					projectRootDirReceived = true;
					parseParams.m_projectRootDir = argv[i + 1];
					AppendArg(argv[i + 1]);
					++i;
				}
				else
				{
					result = droid::ErrorCode::ConfigurationError;
					break;
				}
			}
			else if (currentArg == k_dx12ShaderCompilerCmdLineArg)
			{
				if (i + 1 < argc && argv[i + 1][0] != *k_delimiterChar)
				{
					dx12ShaderCompilerArgReceived = true;
					parseParams.m_directXCompilerExePath = argv[i + 1];
					AppendArg(argv[i + 1]);
					++i;
				}
				else
				{
					result = droid::ErrorCode::ConfigurationError;
					break;
				}
			}
			else if (currentArg == k_dx12TargetProfileArg)
			{
				if (i + 1 < argc && argv[i + 1][0] != *k_delimiterChar)
				{
					parseParams.m_dx12TargetProfile = argv[i + 1];
					AppendArg(argv[i + 1]);
					++i;
				}
				else
				{
					result = droid::ErrorCode::ConfigurationError;
					break;
				}
			}
			else if (currentArg == k_buildConfigCmdLineArg)
			{
				if (i + 1 < argc && argv[i + 1][0] != *k_delimiterChar)
				{
					buildConfigArgReceived = true;
					parseParams.m_buildConfiguration = util::CStrToBuildConfiguration(argv[i + 1]);
					AppendArg(argv[i + 1]);
					++i;
				}
				else
				{
					result = droid::ErrorCode::ConfigurationError;
					break;
				}
			}
			else
			{
				std::cout << "Invalid command line argument: " << currentArg.c_str() << "\n";
				result = droid::ErrorCode::ConfigurationError;
			}
		}

		if (!commandLineArgs.empty())
		{
			std::cout << "Recieved command line args: " << commandLineArgs.c_str() << "\n";
		}
		if (!projectRootDirReceived)
		{
			std::cout << "Project root path not received. Supply \"" << k_projectRootCmdLineArg << 
				" X:\\Path\\To\\SaberEngine\\\" and relaunch.\n";
			result = droid::ErrorCode::ConfigurationError;
		}
		if (!dx12ShaderCompilerArgReceived)
		{
			std::cout << "DX12 shader compiler path not received. Supply \"" << k_dx12ShaderCompilerCmdLineArg <<
				" X:\\Path\\To\\dxc.exe\" and relaunch.\n";
			result = droid::ErrorCode::ConfigurationError;
		}
		if (!buildConfigArgReceived || parseParams.m_buildConfiguration == util::BuildConfiguration::Invalid)
		{
			std::cout << "Build configuration argument not received. Supply \"" << k_buildConfigCmdLineArg <<
				" <config>, with <config> = Debug/DebugRelease/Profile/Release and relaunch.\n";
			result = droid::ErrorCode::ConfigurationError;
		}
	}


	if (result == droid::ErrorCode::Success)
	{
		// Convert paths from relative to absolute:
		parseParams.m_effectSourceDir = parseParams.m_projectRootDir + parseParams.m_effectSourceDir;

		parseParams.m_hlslShaderSourceDir = parseParams.m_projectRootDir + parseParams.m_hlslShaderSourceDir;
		parseParams.m_glslShaderSourceDir = parseParams.m_projectRootDir + parseParams.m_glslShaderSourceDir;
		parseParams.m_commonShaderSourceDir = parseParams.m_projectRootDir + parseParams.m_commonShaderSourceDir;
		parseParams.m_dependenciesDir = parseParams.m_projectRootDir + parseParams.m_dependenciesDir;

		parseParams.m_cppCodeGenOutputDir = parseParams.m_projectRootDir + parseParams.m_cppCodeGenOutputDir;

		parseParams.m_hlslCodeGenOutputDir = parseParams.m_projectRootDir + parseParams.m_hlslCodeGenOutputDir;
		parseParams.m_hlslShaderOutputDir = parseParams.m_projectRootDir + parseParams.m_hlslShaderOutputDir;

		parseParams.m_glslCodeGenOutputDir = parseParams.m_projectRootDir + parseParams.m_glslCodeGenOutputDir;
		parseParams.m_glslShaderOutputDir = parseParams.m_projectRootDir + parseParams.m_glslShaderOutputDir;

		parseParams.m_runtimeEffectsDir = parseParams.m_projectRootDir + parseParams.m_runtimeEffectsDir;

		// Print the final paths we've assembled:
		std::cout << "---\n";
		std::cout << "Current working dir:\t\t\t\"" << parseParams.m_projectRootDir.c_str() << "\"\n";
		std::cout << "Effect source dir:\t\t\t\"" << parseParams.m_effectSourceDir.c_str() << "\"\n";
		
		std::cout << "DirectX shader compiler:\t\t\"" << parseParams.m_directXCompilerExePath.c_str() << "\"\n";
		
		std::cout << "HLSL shader source dir:\t\t\t\"" << parseParams.m_hlslShaderSourceDir.c_str() << "\"\n";
		std::cout << "GLSL shader source dir:\t\t\t\"" << parseParams.m_glslShaderSourceDir.c_str() << "\"\n";
		std::cout << "Common shader source dir:\t\t\"" << parseParams.m_commonShaderSourceDir.c_str() << "\"\n";
		std::cout << "Dependencies shader source dir:\t\t\"" << parseParams.m_dependenciesDir.c_str() << "\"\n";
		
		std::cout << "C++ code generation output path:\t\"" << parseParams.m_cppCodeGenOutputDir.c_str() << "\"\n";
		
		std::cout << "HLSL code generation output path:\t\"" << parseParams.m_hlslCodeGenOutputDir.c_str() << "\"\n";
		std::cout << "HLSL shader compilation output path:\t\"" << parseParams.m_hlslShaderOutputDir.c_str() << "\"\n";
		
		std::cout << "GLSL code generation output path:\t\"" << parseParams.m_glslCodeGenOutputDir.c_str() << "\"\n";
		std::cout << "GLSL shader text output path:\t\t\"" << parseParams.m_glslShaderOutputDir.c_str() << "\"\n";

		std::cout << "Runtime Effect output path:\t\t\"" << parseParams.m_runtimeEffectsDir.c_str() << "\"\n";
		std::cout << "---\n";

		if (doClean)
		{
			if (!shadersOnly)
			{
				std::cout << "Cleaning generated C++ code from \"" << parseParams.m_cppCodeGenOutputDir.c_str() << "\"...\n";
				droid::CleanDirectory(parseParams.m_cppCodeGenOutputDir.c_str());

				std::cout << "Cleaning runtime effects from \"" << parseParams.m_runtimeEffectsDir.c_str() << "\"...\n";
				droid::CleanDirectory(parseParams.m_runtimeEffectsDir.c_str());
			}

			std::cout << "Cleaning generated HLSL code from " << parseParams.m_hlslCodeGenOutputDir.c_str() << "\"...\n";
			droid::CleanDirectory(parseParams.m_hlslCodeGenOutputDir.c_str());
			
			std::cout << "Cleaning HLSL shaders from " << parseParams.m_hlslShaderOutputDir.c_str() << "\"...\n";
			droid::CleanDirectory(parseParams.m_hlslShaderOutputDir.c_str());

			std::cout << "Cleaning generated GLSL code from " << parseParams.m_glslCodeGenOutputDir.c_str() << "\"...\n";
			droid::CleanDirectory(parseParams.m_glslCodeGenOutputDir.c_str());

			std::cout << "Cleaning GLSL shaders from " << parseParams.m_glslShaderOutputDir.c_str() << "\"...\n";
			droid::CleanDirectory(parseParams.m_glslShaderOutputDir.c_str());

			std::cout << "Cleaning done!\n---\n";
		}
		if (doBuild)
		{
			result = droid::DoParsingAndCodeGen(parseParams);
		}
	}

	std::cout << std::format(
		"\nDroid resource burning {} with code \"{}\"\n",
		result >= 0 ? "completed" : "failed",
		droid::ErrorCodeToCStr(result)).c_str();

	return result >= 0 ? droid::ErrorCode::Success : result;
}

