// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "FileWriter.h"
#include "ParseDB.h"
#include "TextStrings.h"

#include "Core/Definitions/ConfigKeys.h"
#include "Core/Definitions/EffectKeys.h"


namespace droid
{
	ParseDB::ParseDB(ParseParams const& parseParams)
		: m_parseParams(parseParams)
	{		
	}


	droid::ErrorCode ParseDB::Parse()
	{
		// Skip parsing if the generated code was modified more recently than the effect files:
		const time_t effectsDirModificationTime = GetMostRecentlyModifiedFileTime(m_parseParams.m_effectsDir);
		const time_t codeGenDirModificationTime = GetMostRecentlyModifiedFileTime(m_parseParams.m_codeGenOutputDir);
		if (codeGenDirModificationTime > effectsDirModificationTime)
		{
			return droid::ErrorCode::NoModification;
		}

		std::string const& effectManifestPath = std::format("{}{}",
			m_parseParams.m_effectsDir,
			m_parseParams.m_effectManifestFileName);
			
		std::cout << "\nLoading effect manifest \"" << effectManifestPath.c_str() << "\"...\n";

		std::ifstream effectManifestInputStream(effectManifestPath);
		if (!effectManifestInputStream.is_open())
		{
			std::cout << "Error: Failed to open effect manifest input stream\n";
			return droid::ErrorCode::FileError;
		}
		std::cout << "Successfully opened effect manifest \"" << effectManifestPath.c_str() << "\"!\n\n";

		droid::ErrorCode result = droid::ErrorCode::Success;

		std::vector<std::string> effectNames;
		nlohmann::json effectManifestJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			effectManifestJSON = nlohmann::json::parse(
				effectManifestInputStream,
				parserCallback,
				m_parseParams.m_allowJSONExceptions,
				m_parseParams.m_ignoreJSONComments);

			// Build the list of effect names:
			for (auto const& effectManifestEntry : effectManifestJSON.at(key_effectsBlock))
			{
				std::string const& effectName = effectManifestEntry.template get<std::string>();
				effectNames.emplace_back(effectName);
			}

			std::cout << "Effect manifest successfully parsed!\n\n";
			effectManifestInputStream.close();
		}
		catch (nlohmann::json::exception parseException)
		{
			std::cout << std::format(
				"Failed to parse the Effect manifest file \"{}\"\n{}",
				effectManifestPath,
				parseException.what()).c_str();

			effectManifestInputStream.close();

			result = ErrorCode::JSONError;
		}

		// Parse the effect files listed in the manifest:
		for (auto const& effectName : effectNames)
		{
			result = ParseEffectFile(effectName, m_parseParams);
			if (result != droid::ErrorCode::Success)
			{
				break;
			}
		}

		return result;
	}


	time_t ParseDB::GetMostRecentlyModifiedFileTime(std::string const& filesystemTarget)
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


	droid::ErrorCode ParseDB::ParseEffectFile(std::string const& effectName, ParseParams const& parseParams)
	{
		std::cout << "Parsing Effect \"" << effectName.c_str() << "\":\n";
		
		std::string const& effectFileName = effectName + ".json";
		std::string const& effectFilePath = parseParams.m_effectsDir + effectFileName;

		std::ifstream effectInputStream(effectFilePath);
		if (!effectInputStream.is_open())
		{
			std::cout << "Error: Failed to open effect input stream" << effectFilePath << "\n";
			return droid::ErrorCode::FileError;
		}
		std::cout << "Successfully opened effect file \"" << effectFilePath.c_str() << "\"!\n\n";

		droid::ErrorCode result = droid::ErrorCode::Success;

		nlohmann::json effectJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			effectJSON = nlohmann::json::parse(
				effectInputStream,
				parserCallback,
				parseParams.m_allowJSONExceptions,
				parseParams.m_ignoreJSONComments);

			// "Effect":
			if (effectJSON.contains(key_effectBlock))
			{
				auto const& effectBlock = effectJSON.at(key_effectBlock);

				// "DrawStyles":
				if (effectBlock.contains(key_drawStyles))
				{
					for (auto const& drawstyleBlock : effectBlock.at(key_drawStyles))
					{
						// "Conditions":"
						if (drawstyleBlock.contains(key_conditions))
						{
							for (auto const& condition : drawstyleBlock.at(key_conditions))
							{
								std::string rule;
								if (condition.contains(key_rule))
								{
									rule = condition.at(key_rule);
								}
								
								std::string mode;
								if (condition.contains(key_mode))
								{
									mode = condition.at(key_mode);
								}

								if (!rule.empty() && !mode.empty())
								{
									AddDrawstyle(rule, mode);
								}
							}
						}
					}
				}
			}

			std::cout << "Effect file successfully parsed!\n\n";
		}
		catch (nlohmann::json::exception parseException)
		{
			std::cout << std::format(
				"Failed to parse the Effect file \"{}\"\n{}",
				effectName,
				parseException.what()).c_str();

			result = ErrorCode::JSONError;
		}

		effectInputStream.close();
		return result;
	}


	droid::ErrorCode ParseDB::GenerateCPPCode() const
	{
		droid::ErrorCode result = GenerateDrawstyleCPPCode();
		if (result != droid::ErrorCode::Success)
		{
			return result;
		}

		return result;
	}


	droid::ErrorCode ParseDB::GenerateDrawstyleCPPCode() const
	{
		FileWriter filewriter(m_parseParams.m_codeGenOutputDir, m_drawstyleHeaderName);

		droid::ErrorCode result = filewriter.GetStatus();
		if (result != droid::ErrorCode::Success)
		{
			return filewriter.GetStatus();
		}

		filewriter.OpenNamespace("effect::drawstyle");

		filewriter.WriteLine("using Bitmask = uint64_t;\n");

		filewriter.WriteLine("constexpr Bitmask DefaultTechnique = 0;");
		
		uint8_t bitIdx = 0;
		for (auto const& drawstyle : m_drawstyles)
		{
			for (auto const& rule : drawstyle.second)
			{

				filewriter.WriteLine(std::format("constexpr Bitmask {}_{} = 1llu << {};", 
					drawstyle.first, rule, bitIdx++));
			}
		}
		if (bitIdx >= 64)
		{
			std::cout << "Error: " << " is too many drawstyle rules to fit in a 64-bit bitmask\n";
			result = droid::ErrorCode::DataError;
		}

		filewriter.CloseNamespace();

		return result;
	}
}