// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"

#include "Definitions\KeyConfiguration.h"

#include "Util\TextUtils.h"

#define SET_CMD		"set"		// Set a value
#define BIND_CMD	"bind"		// Bind a key


namespace
{
	// Convert a std::string to lower case: Used to simplify comparisons
	inline std::string ToLowerCase(std::string input)
	{
		std::string output;
		for (auto const& currentChar : input)
		{
			output += std::tolower(currentChar);
		}
		return output;
	}
}

namespace core
{
	Config* Config::Get()
	{
		static std::unique_ptr<core::Config> instance = std::make_unique<core::Config>();
		return instance.get();
	}


	Config::Config()
		: m_isDirty(false)
	{
		// Insert engine defaults:
		SetValue<std::string>(core::configkeys::k_scenesDirNameKey, "Scenes\\", Config::SettingType::Runtime);

		SetValue<bool>(core::configkeys::k_jsonAllowExceptionsKey, true, Config::SettingType::Runtime);
		SetValue<bool>(core::configkeys::k_jsonIgnoreCommentsKey, true, Config::SettingType::Runtime);
	}


	Config::~Config()
	{
		SaveConfigFile();
	}


	void Config::ProcessCommandLineArgs(int argc, char** argv)
	{
		// NOTE: This is one of the first functions run at startup; We cannot use the LogManager yet

		constexpr char k_keyDelimiter = '-'; // Signifies a -key (e.g. -scene mySceneName)
		auto StripKeyDelimiter = [](std::string const& key) -> char const*
			{
				const size_t keyStartIdx = key.find_first_not_of(k_keyDelimiter);
				return &key.c_str()[keyStartIdx];
			};

		struct KeyValue
		{
			const std::string m_key;
			std::string m_value;

			bool HasValue() const { return !m_value.empty(); }
		};
		std::vector<KeyValue> keysValues;
		keysValues.reserve(argc - 1);

		// Pre-parse the args into key/value pairs:
		std::string argString; // The full list of all command line args received		
		for (int i = 1; i < argc; i++)
		{
			std::string currentToken = argv[i];

			// Append the current token to our argument std::string:
			argString += std::format("{}{}",
				currentToken,
				i + 1 < argc ? " " : ""); // Don't add a space if it's the last token

			auto CurrentTokenIsKey = [&]()
				{
					return currentToken.find_first_of(k_keyDelimiter) != std::string::npos;
				};

			if (CurrentTokenIsKey())
			{
				keysValues.emplace_back(KeyValue{
					StripKeyDelimiter(argv[i]),
					""}); // Empty, until we check the next token and see it's a value
			}
			else
			{
				if (!keysValues.back().m_value.empty())
				{
					LOG_ERROR(std::format(
						"Invalid command line argument key/value sequence: Value \"{}\" overridden with \"{}\"",
						keysValues.back().m_value,
						currentToken).c_str());
				}
				keysValues.back().m_value = std::move(currentToken);
			}
		}

		// Store the received command line std::string
		SetValue(core::configkeys::k_commandLineArgsValueKey, argString, Config::SettingType::Runtime);

		// Process the key/value pairs:
		for (size_t i = 0; i < keysValues.size(); i++)
		{
			if (keysValues[i].HasValue())
			{
				bool isNumericValue = true;
				try
				{
					int numericValue = std::stoi(keysValues[i].m_value);
					SetValue(util::HashKey::Create(keysValues[i].m_key.c_str()), numericValue, Config::SettingType::Runtime);
				}
				catch (std::invalid_argument)
				{
					isNumericValue = false;
				}
				if (!isNumericValue)
				{
					SetValue(util::HashKey::Create(keysValues[i].m_key), keysValues[i].m_value, Config::SettingType::Runtime);
				}
			}
			else
			{
				// If no value was provided with a key, just set it as a boolean flag
				SetValue(util::HashKey::Create(keysValues[i].m_key), true, Config::SettingType::Runtime);
			}
		}

		// Post-processing:
		if (KeyExists(util::HashKey(core::configkeys::k_sceneCmdLineArg)))
		{
			std::string const& sceneDirName = GetValue<std::string>(core::configkeys::k_scenesDirNameKey); // "Scenes\\"
			std::string const& extractedSceneArg = GetValue<std::string>(core::configkeys::k_sceneCmdLineArg);
			
			// Assemble the relative scene file path:
			const std::string sceneFilePath = sceneDirName + extractedSceneArg; // == "Scenes\Some\Folder\Names\file.ext"
			SetValue(core::configkeys::k_sceneFilePathKey, sceneFilePath, Config::SettingType::Runtime);

			// sceneRootPath == ".\Scenes\Scene\Folder\Names\":
			const size_t lastSlash = sceneFilePath.find_last_of("\\");
			const std::string sceneRootPath = sceneFilePath.substr(0, lastSlash) + "\\";
			SetValue(core::configkeys::k_sceneRootPathKey, sceneRootPath, Config::SettingType::Runtime);

			// sceneName == "sceneFile"
			const std::string filenameAndExt = sceneFilePath.substr(lastSlash + 1, sceneFilePath.size() - lastSlash);
			const size_t extensionPeriod = filenameAndExt.find_last_of(".");
			const std::string sceneName = filenameAndExt.substr(0, extensionPeriod);
			SetValue(core::configkeys::k_sceneNameKey, sceneName, Config::SettingType::Runtime);

			// sceneIBLDir == ".\Scenes\SceneFolderName\IBL\"
			std::string const& sceneIBLDir = sceneRootPath + "IBL\\";
			SetValue(core::configkeys::k_sceneIBLDirKey, sceneIBLDir, Config::SettingType::Runtime);

			// sceneIBLPath == ".\Scenes\SceneFolderName\IBL\ibl.hdr"
			const std::string sceneIBLPath = sceneIBLDir + "ibl.hdr";
			SetValue(core::configkeys::k_sceneIBLPathKey, sceneIBLPath, Config::SettingType::Runtime);
		}

		// We don't count command line arg entries as dirtying the config
		m_isDirty = false;
	}


	void Config::LoadConfigFile()
	{
		// Before we load the config, pre-populate it with default values
		InitializeOSValues();
		InitializeDefaultValues();
		SetRuntimeDefaults();
		m_isDirty = false; // Don't consider setting defaults as dirtying the config

		LOG("Loading %s...", core::configkeys::k_configFileName);

		std::ifstream file;
		file.open(std::format("{}{}", core::configkeys::k_configDirName, core::configkeys::k_configFileName).c_str());

		// If no config is found, create one:
		const bool foundExistingConfig = file.is_open();
		if (!foundExistingConfig)
		{
			LOG_WARNING("No %s file found! Attempting to create a default version", core::configkeys::k_configFileName);
			m_isDirty = true;
			SaveConfigFile();
		}

		// Process the config file:
		std::string line;
		bool foundInvalidString = false;
		while (file.good())
		{
			// Handle malformed std::strings from previous iteration:
			if (foundInvalidString == true)
			{
				LOG_WARNING("Ignoring invalid command in %s:\n%s", core::configkeys::k_configFileName, line);
				foundInvalidString = false;
			}

			// Get the next line:
			getline(file, line);

			// Replace whitespace with single spaces:
			std::regex tabMatch("([\\s])+");
			std::string cleanLine = std::regex_replace(line, tabMatch, " ");

			// Skip empty or near-empty lines:
			if (cleanLine.find_first_not_of(" \t\n") == std::string::npos || cleanLine.length() <= 2)
			{
				continue;
			}

			// Remove single leading space, if it exists:
			if (cleanLine.at(0) == ' ')
			{
				cleanLine = cleanLine.substr(1, std::string::npos);
			}

			// Remove comments:
			size_t commentStart = cleanLine.find_first_of("#");
			if (commentStart != std::string::npos)
			{
				// Remove the trailing space, if it exists:
				if (commentStart > size_t(0) && cleanLine.at(commentStart - 1) == ' ')
				{
					commentStart--;
				}

				cleanLine = cleanLine.substr(0, commentStart);

				if (cleanLine.length() == 0)
				{
					continue;
				}
			}

			// Ensure we have exactly 3 arguments:
			int numSpaces = 0;
			for (int i = 0; i < cleanLine.length(); i++)
			{
				if (cleanLine.at(i) == ' ')
				{
					numSpaces++;
				}
			}
			if (numSpaces < 2)
			{
				foundInvalidString = true;
				continue;
			}

			// Extract leading command:
			size_t firstSpace = cleanLine.find_first_of(" \t", 1);
			std::string command = cleanLine.substr(0, firstSpace);

			// Remove the command from the head of the std::string:
			cleanLine = cleanLine.substr(firstSpace + 1, std::string::npos);

			// Extract the variable property name:
			firstSpace = cleanLine.find_first_of(" \t\n", 1);
			std::string const& property = cleanLine.substr(0, firstSpace);

			// Remove the property from the head of the std::string:
			cleanLine = cleanLine.substr(firstSpace + 1, std::string::npos);

			// Clean up the value std::string:
			std::string value = cleanLine;

			// Remove quotation marks from value std::string:
			bool isString = false;
			if (value.find("\"") != std::string::npos)
			{
				isString = true;
				std::regex quoteMatch("([\\\"])+");
				value = std::regex_replace(value, quoteMatch, "");
			}

			// Update config hashtables. We set all SettingsType as common, to ensure otherwise API-specific settings
			// will be written to disk
			if (command == SET_CMD)
			{
				// std::strings:
				if (isString)
				{
					SetValue(util::HashKey::Create(property), std::string(value), SettingType::Serialized);
				}
				else
				{
					// Booleans:
					std::string const& boolString = ToLowerCase(value);
					if (boolString == k_trueString)
					{
						SetValue(util::HashKey::Create(property), true, SettingType::Serialized);
						continue;
					}
					else if (boolString == k_falseString)
					{
						SetValue(util::HashKey::Create(property), false, SettingType::Serialized);
						continue;
					}

					// TODO: Handle std::strings without "quotations" -> If it doesn't contain numbers, assume it's a std::string

					// Numeric values: Try and cast as an int, and fallback to a float if it fails
					size_t position = 0;
					int intResult = std::stoi(value, &position);

					// Ints:
					if (position == value.length())
					{
						SetValue(util::HashKey::Create(property), intResult, SettingType::Serialized);
					}
					else // Floats:
					{
						float floatResult = std::stof(value);
						SetValue(util::HashKey::Create(property), floatResult, SettingType::Serialized);
					}
				}
			}
			else if (command == "bind")
			{
				if (isString)
				{
					SetValue(util::HashKey::Create(property), std::string(value), SettingType::Serialized);
				}
				else
				{
					// Assume bound values are single chars, for now. Might need to rework this to bind more complex keys
					SetValue(util::HashKey::Create(property), (char)value[0], SettingType::Serialized);
				}
			}
			else
			{
				foundInvalidString = true;
				continue;
			}
		}

		// Handle final malformed std::string:
		if (foundInvalidString == true)
		{
			LOG_WARNING("Ignoring invalid command in %s:\n%s", core::configkeys::k_configFileName, line);
		}

		// We don't count existing entries as dirtying the config
		m_isDirty |= !foundExistingConfig;

		SaveConfigFile(); // Write out the results immediately

		LOG("Done!");
	}


	void Config::InitializeOSValues()
	{
		// The "My Documents" folder:
		wchar_t* documentsFolderPathPtr = nullptr;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documentsFolderPathPtr);
		const std::string documentFolderPath = util::FromWideString(documentsFolderPathPtr);
		CoTaskMemFree(static_cast<void*>(documentsFolderPathPtr));

		SetValue(core::configkeys::k_documentsFolderPathKey, documentFolderPath, SettingType::Runtime);
	}


	void Config::InitializeDefaultValues()
	{	
		// Window:
		SetValue(core::configkeys::k_windowTitleKey, std::string("Saber Engine"), SettingType::Serialized);
		SetValue(core::configkeys::k_windowWidthKey,	1920,	SettingType::Serialized);
		SetValue(core::configkeys::k_windowHeightKey,	1080,	SettingType::Serialized);

		// System config:
		SetValue(core::configkeys::k_vsyncEnabledKey,	true,	SettingType::Serialized);

		// Texture dimensions:
		SetValue(core::configkeys::k_defaultDirectionalShadowMapResolutionKey,	2048,	SettingType::Serialized);
		SetValue(core::configkeys::k_defaultShadowCubeMapResolutionKey,			512,	SettingType::Serialized);
		SetValue(core::configkeys::k_defaultSpotShadowMapResolutionKey,			1024,	SettingType::Serialized);

		// Quality settings:
		SetValue(core::configkeys::k_brdfLUTWidthHeightKey,		1024,	SettingType::Serialized);
		SetValue(core::configkeys::k_iemTexWidthHeightKey,		512,	SettingType::Serialized);
		SetValue(core::configkeys::k_iemNumSamplesKey,			4096,	SettingType::Serialized);
		SetValue(core::configkeys::k_pmremTexWidthHeightKey,	1024,	SettingType::Serialized);
		SetValue(core::configkeys::k_pmremNumSamplesKey,		4096,	SettingType::Serialized);

		// Camera defaults:
		SetValue(core::configkeys::k_defaultFOVKey,		1.570796f,	SettingType::Serialized);
		SetValue(core::configkeys::k_defaultNearKey,	1.0f,		SettingType::Serialized);
		SetValue(core::configkeys::k_defaultFarKey,		100.0f,		SettingType::Serialized);

		// Input parameters:
		SetValue(core::configkeys::k_mousePitchSensitivityKey,		0.5f,	SettingType::Serialized);
		SetValue(core::configkeys::k_mouseYawSensitivityKey,		0.5f,	SettingType::Serialized);
		SetValue(core::configkeys::k_sprintSpeedModifierKey,		2.0f,	SettingType::Serialized);

		// Scene data:
		SetValue(core::configkeys::k_defaultEngineIBLPathKey,	"Assets\\DefaultIBL\\default.hdr",	SettingType::Serialized);

		// Key bindings:
		//--------------
		SetValue(ENUM_TO_STR(InputButton_Forward),	'w',			SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputButton_Backward),	's',			SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputButton_Left),		'a',			SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputButton_Right),	'd',			SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputButton_Up),		"Space",		SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputButton_Down),		"Left Shift",	SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputButton_Sprint),	"Left Ctrl",	SettingType::Serialized);

		SetValue(ENUM_TO_STR(InputButton_Console),	"Grave",		SettingType::Serialized); // The "grave accent"/tilde key: `
		SetValue(ENUM_TO_STR(InputButton_VSync),	'v',			SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputButton_Quit),		"Escape",		SettingType::Serialized);

		// Mouse bindings:
		SetValue(ENUM_TO_STR(InputMouse_Left),	ENUM_TO_STR(InputMouse_Left),	SettingType::Serialized);
		SetValue(ENUM_TO_STR(InputMouse_Right),	ENUM_TO_STR(InputMouse_Right),	SettingType::Serialized);
	}


	void Config::SetRuntimeDefaults()
	{
		auto SetRuntimeValue = [&](util::HashKey const& key, auto const& value)
		{
			SetValue(key, value, SettingType::Runtime);
		};

		// Debug:
		SetRuntimeValue(core::configkeys::k_debugLevelCmdLineArg, 0);

		// Shadow map defaults:
		SetRuntimeValue(core::configkeys::k_defaultDirectionalLightMinShadowBiasKey,	0.012f);
		SetRuntimeValue(core::configkeys::k_defaultDirectionalLightMaxShadowBiasKey,	0.035f);
		SetRuntimeValue(core::configkeys::k_defaultDirectionalLightShadowSoftnessKey,	0.02f);
		SetRuntimeValue(core::configkeys::k_defaultPointLightMinShadowBiasKey,			0.03f);
		SetRuntimeValue(core::configkeys::k_defaultPointLightMaxShadowBiasKey,			0.055f);
		SetRuntimeValue(core::configkeys::k_defaultPointLightShadowSoftnessKey,			0.1f);
		SetRuntimeValue(core::configkeys::k_defaultSpotLightMinShadowBiasKey,			0.03f);
		SetRuntimeValue(core::configkeys::k_defaultSpotLightMaxShadowBiasKey,			0.055f);
		SetRuntimeValue(core::configkeys::k_defaultSpotLightShadowSoftnessKey,			0.1f);
	}
	

	bool Config::KeyExists(util::HashKey const& valueName) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_configValuesMutex);

			auto const& result = m_configValues.find(valueName);
			return result != m_configValues.end();
		}
	}


	std::string Config::GetValueAsString(util::HashKey const& valueName) const
	{
		std::string returnVal;
		{
			std::shared_lock<std::shared_mutex> readLock(m_configValuesMutex);

			auto const& result = m_configValues.find(valueName);

			if (result != m_configValues.end())
			{
				try
				{
					if (result->second.first.type() == typeid(std::string))
					{
						returnVal = any_cast<std::string>(result->second.first);
					}
					else if (result->second.first.type() == typeid(char const*))
					{
						returnVal = std::string(any_cast<char const*>(result->second.first));
					}
					else if (result->second.first.type() == typeid(float))
					{
						float configValue = any_cast<float>(result->second.first);
						returnVal = std::to_string(configValue);
					}
					else if (result->second.first.type() == typeid(int))
					{
						int configValue = any_cast<int>(result->second.first);
						returnVal = std::to_string(configValue);
					}
					else if (result->second.first.type() == typeid(char))
					{
						char configValue = any_cast<char>(result->second.first);
						returnVal = std::string(1, configValue); // Construct a std::string with 1 element
					}
					else if (result->second.first.type() == typeid(bool))
					{
						bool configValue = any_cast<bool>(result->second.first);
						returnVal = configValue == true ? "1" : "0";
					}
				}
				catch (const std::bad_any_cast& e)
				{
					LOG_ERROR("bad_any_cast exception thrown: Invalid type requested from Config\n%s", e.what());
				}
			}
			else
			{
				LOG_ERROR("Config key \"%s\" does not exist\n", valueName.GetKey());
			}
		}

		return returnVal;
	}


	std::wstring Config::GetValueAsWString(util::HashKey const& valueName) const
	{
		std::string const& result = GetValueAsString(valueName);
		return util::ToWideString(result);
	}


	void Config::SaveConfigFile()
	{
		if (m_isDirty == false)
		{
			LOG("SaveConfigFile called, but config has not changed. Returning without modifying file on disk");
			return;
		}
		LOG("Saving %s...", core::configkeys::k_configFileName);

		// Create the .\config\ directory, if none exists
		std::filesystem::path configPath = core::configkeys::k_configDirName;
		if (!std::filesystem::exists(configPath))
		{
			LOG("Creating .\\config\\ directory");

			std::filesystem::create_directory(configPath);
		}

		{
			std::shared_lock<std::shared_mutex> readLock(m_configValuesMutex);

			// Build a list of the std::strings we plan to write, so we can sort them
			struct ConfigEntry
			{
				std::string m_cmdPrefix; // SET_CMD, BIND_CMD
				std::string m_key;
				std::string m_value;
			};
			std::vector<ConfigEntry> configEntries;
			configEntries.reserve(m_configValues.size());

			for (auto const& currentElement : m_configValues)
			{
				if (currentElement.second.second == SettingType::Runtime)
				{
					continue;	// Skip API-specific settings
				}

				SEAssert(currentElement.first.GetKey() != nullptr, "Found a HashKey with a null key string");

				if (currentElement.second.first.type() == typeid(std::string) &&
					strstr(currentElement.first.GetKey(), "Input") == nullptr)
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = SET_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString<std::string const&>(
							any_cast<std::string const&>(currentElement.second.first)) });
				}
				else if (currentElement.second.first.type() == typeid(char const*) &&
					strstr(currentElement.first.GetKey(), "Input") == nullptr)
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = SET_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString(any_cast<char const*>(currentElement.second.first)) });
				}
				else if (currentElement.second.first.type() == typeid(float))
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = SET_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString(any_cast<float>(currentElement.second.first)) });
				}
				else if (currentElement.second.first.type() == typeid(int))
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = SET_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString(any_cast<int>(currentElement.second.first)) });
				}
				else if (currentElement.second.first.type() == typeid(bool))
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = SET_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString(any_cast<bool>(currentElement.second.first)) });
				}
				else if (currentElement.second.first.type() == typeid(char))
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = BIND_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString(any_cast<char>(currentElement.second.first)) });
				}
				else if (currentElement.second.first.type() == typeid(std::string) &&
					strstr(currentElement.first.GetKey(), "Input") != nullptr)
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = BIND_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString<std::string const&>(
							any_cast<std::string const&>(currentElement.second.first)) });
				}
				else if (currentElement.second.first.type() == typeid(char const*) &&
					strstr(currentElement.first.GetKey(), "Input") != nullptr)
				{
					configEntries.emplace_back(ConfigEntry{
						.m_cmdPrefix = BIND_CMD,
						.m_key = currentElement.first.GetKey(),
						.m_value = PropertyToConfigString(any_cast<char const*>(currentElement.second.first)) });
				}
				else
				{
					LOG_ERROR("Cannot write unsupported type to config");
				}
			}

			// Sort the results:
			std::sort(configEntries.begin(), configEntries.end(), [](ConfigEntry const& a, ConfigEntry const& b) {
				int cmpResult = strcmp(a.m_cmdPrefix.c_str(), b.m_cmdPrefix.c_str()); // Compare command types
				if (cmpResult == 0)
				{
					cmpResult = strcmp(a.m_key.c_str(), b.m_key.c_str()); // Tie breaker: Compare keys
				}
				return cmpResult < 0; });

			// Write our config to disk:
			std::ofstream config_ofstream(
				std::format("{}{}", core::configkeys::k_configDirName, core::configkeys::k_configFileName));
			config_ofstream << std::format("# SaberEngine {} file:\n", core::configkeys::k_configFileName).c_str();

			for (ConfigEntry const& currentEntry : configEntries)
			{
				config_ofstream << currentEntry.m_cmdPrefix << " " << currentEntry.m_key << currentEntry.m_value;
			}

			m_isDirty = false;
		}
	}
}