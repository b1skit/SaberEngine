// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "ParseDB.h"
#include "TextStrings.h"

#include "Core/Definitions/ConfigKeys.h"


// TODO: Make these controllable via command line args
constexpr bool k_jsonAllowExceptions = false;
constexpr bool k_jsonIgnoreComments = true;


int main(int argc, char* argv[])
{
	std::cout << droid::k_logHeader;
	std::cout << "Launching...\n";

	droid::ErrorCode result = droid::ErrorCode::Success;

	droid::ParseParams parseParams{
		.m_workingDirectory = std::filesystem::current_path().string() + "\\",
		.m_appDirectory = core::configkeys::k_appDirName,
		.m_effectsDir = std::format("{}{}", core::configkeys::k_appDirName, core::configkeys::k_effectDirName),
		.m_codeGenOutputDir = "Source\\Generated\\",

		.m_effectManifestFileName = core::configkeys::k_effectManifestFilename,
	};

	// Handle command line args:
	bool doClean = false;
	bool doBuild = true;
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
			else if (currentArg == "-workingdir")
			{
				if (i + 1 < argc)
				{
					parseParams.m_workingDirectory = argv[i + 1];
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
				std::cout << "Invalid command line argument: " << currentArg.c_str();
				result = droid::ErrorCode::ConfigurationError;
				break;
			}
		}

		if (!commandLineArgs.empty())
		{
			std::cout << "Recieved command line args: " << commandLineArgs.c_str() << "\n";
		}
	}

	if (result == droid::ErrorCode::Success)
	{
		// Convert paths from relative to absolute:
		parseParams.m_effectsDir = parseParams.m_workingDirectory + parseParams.m_effectsDir;
		parseParams.m_codeGenOutputDir = parseParams.m_workingDirectory + parseParams.m_codeGenOutputDir;

		std::cout << "---\n";
		std::cout << "Current working directory:\t\"" << parseParams.m_workingDirectory.c_str() << "\"\n";
		std::cout << "Effect directory:\t\t\"" << parseParams.m_effectsDir.c_str() << "\"\n";
		std::cout << "Code generation output path:\t\"" << parseParams.m_codeGenOutputDir.c_str() << "\"\n";
		std::cout << "---\n";

		if (doClean)
		{
			std::cout << "Cleaning generated code...\n";

			std::filesystem::remove_all(parseParams.m_codeGenOutputDir.c_str());
		}
		if (doBuild)
		{
			result = droid::DoParsingAndCodeGen(parseParams);

			std::cout << std::format(
				"\nDroid resource burning {}\nResult: \"{}\"\n",
				result >= 0 ? "complete!" : "failed!",
				droid::ErrorCodeToCStr(result)).c_str();
		}
	}

	return result >= 0 ? droid::ErrorCode::Success : result;
}

