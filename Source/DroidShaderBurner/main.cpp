// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "ParseDB.h"
#include "TextStrings.h"

#include "Core/Definitions/ConfigKeys.h"


// TODO: Make these controllable via command line args
constexpr bool k_jsonAllowExceptions = false;
constexpr bool k_jsonIgnoreComments = true;

constexpr char const* k_delimiterChar = "-";
constexpr char const* k_projectRootCommandLineArg = "-projectroot";


int main(int argc, char* argv[])
{
	std::cout << droid::k_logHeader;
	std::cout << "Launching...\n";

	droid::ErrorCode result = droid::ErrorCode::Success;

	droid::ParseParams parseParams{
		// Paths:
		.m_projectRootDir		= "PROJECT_ROOT_DIRECTORY_NOT_SET", // Mandatory command line arg
		.m_appDir				= core::configkeys::k_appDirName,
		.m_effectsDir			= std::format("{}{}", core::configkeys::k_appDirName, core::configkeys::k_effectDirName),
		.m_cppCodeGenOutputDir	= "Source\\Generated\\",
		.m_hlslCodeGenOutputDir = "Source\\Shaders\\Generated\\HLSL\\",
		.m_glslCodeGenOutputDir = "Source\\Shaders\\Generated\\GLSL\\",

		// File names:
		.m_effectManifestFileName = core::configkeys::k_effectManifestFilename,
	};

	// Handle command line args:
	bool doClean = false;
	bool doBuild = true;
	bool projectRootDirReceived = false;
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

			if (currentArg == "-disallowjsonexceptions")
			{
				parseParams.m_allowJSONExceptions = false;
			}
			else if (currentArg == "-disallowjsoncomments")
			{
				parseParams.m_ignoreJSONComments = false;
			}
			else if (currentArg == "-clean")
			{
				doClean = true;
				doBuild = false;
			}
			else if (currentArg == "-cleanandrebuild")
			{
				doClean = true;
				doBuild = true;
			}
			else if (currentArg == k_projectRootCommandLineArg)
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
			std::cout << "Project root command not received. Supply \"" << k_projectRootCommandLineArg << 
				" X:\\Path\\To\\SaberEngine\\\" and relaunch.\n";
			result = droid::ErrorCode::ConfigurationError;
		}
	}


	if (result == droid::ErrorCode::Success)
	{
		// Convert paths from relative to absolute:
		parseParams.m_effectsDir = parseParams.m_projectRootDir + parseParams.m_effectsDir;
		parseParams.m_cppCodeGenOutputDir = parseParams.m_projectRootDir + parseParams.m_cppCodeGenOutputDir;
		parseParams.m_hlslCodeGenOutputDir = parseParams.m_projectRootDir + parseParams.m_hlslCodeGenOutputDir;
		parseParams.m_glslCodeGenOutputDir = parseParams.m_projectRootDir + parseParams.m_glslCodeGenOutputDir;

		// Print the final paths we've assembled:
		std::cout << "---\n";
		std::cout << "Current working directory:\t\t\"" << parseParams.m_projectRootDir.c_str() << "\"\n";
		std::cout << "Effect directory:\t\t\t\"" << parseParams.m_effectsDir.c_str() << "\"\n";
		std::cout << "C++ code generation output path:\t\"" << parseParams.m_cppCodeGenOutputDir.c_str() << "\"\n";
		std::cout << "HLSL code generation output path:\t\"" << parseParams.m_hlslCodeGenOutputDir.c_str() << "\"\n";
		std::cout << "GLSL code generation output path:\t\"" << parseParams.m_glslCodeGenOutputDir.c_str() << "\"\n";
		std::cout << "---\n";

		if (doClean)
		{
			std::cout << "Cleaning generated code...\n";

			droid::CleanDirectory(parseParams.m_cppCodeGenOutputDir.c_str());
			droid::CleanDirectory(parseParams.m_hlslCodeGenOutputDir.c_str());
			droid::CleanDirectory(parseParams.m_glslCodeGenOutputDir.c_str());
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

