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

	std::filesystem::path cwd = std::filesystem::current_path();
	std::cout << "Current working directory: \"" << cwd.string().c_str() << "\"\n";

	droid::ParseParams parseParams{
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

			commandLineArgs += argv[i];
			if (i + 1 < argc)
			{
				commandLineArgs += " ";
			}
		}
		std::cout << "Recieved command line args: " << commandLineArgs.c_str() << "\n";
	}

	droid::ErrorCode result = droid::ErrorCode::Success;

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

	return result;
}

