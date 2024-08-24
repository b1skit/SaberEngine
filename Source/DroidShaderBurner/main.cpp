// © 2024 Adam Badke. All rights reserved.
#include "ParseDB.h"
#include "ParseEffect.h"
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
		.m_effectsDir = std::format("SaberEngine\\{}", core::configkeys::k_effectDirName),
		.m_effectManifestPath = std::format(
			"SaberEngine\\{}{}", core::configkeys::k_effectDirName, core::configkeys::k_effectManifestFilename),
		.m_codeGenPath = "Source\\Generated\\",
	};

	// Handle command line args:
	bool doClean = false;
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
		parseParams.m_effectManifestPath = parseParams.m_workingDirectory + parseParams.m_effectManifestPath;
		parseParams.m_codeGenPath = parseParams.m_workingDirectory + parseParams.m_codeGenPath;


		std::cout << "---\n";
		std::cout << "Current working directory:\t\"" << parseParams.m_workingDirectory.c_str() << "\"\n";
		std::cout << "Effect file directory:\t\t\"" << parseParams.m_effectsDir.c_str() << "\"\n";
		std::cout << "Effect manifest file path:\t\"" << parseParams.m_effectManifestPath.c_str() << "\"\n";
		std::cout << "Code generation output path:\t\"" << parseParams.m_codeGenPath.c_str() << "\"\n";
		std::cout << "---\n";

		if (doClean)
		{
			std::cout << "Cleaning generated code...\n";

			std::filesystem::remove_all(parseParams.m_codeGenPath.c_str());
		}
		else
		{
			result = droid::DoParsingAndCodeGen(parseParams);

			std::cout << std::format(
				"\nDroid shader burning complete!\nResult: \"{}\"\n", droid::ErrorCodeToCStr(result)).c_str();
		}
	}

	return result;
}

