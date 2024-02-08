// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "KeyConfiguration.h"
#include "TextUtils.h"

// Default true/false strings. We convert config values to lowercase and compare against these
#define TRUE_STRING		"true"
#define FALSE_STRING	"false"

#define SET_CMD		"set"		// Set a value
#define BIND_CMD	"bind"		// Bind a key

using std::ifstream;
using std::any_cast;
using std::string;
using std::to_string;
using std::unordered_map;
using std::any;


namespace
{
	// Convert a string to lower case: Used to simplify comparisons
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

namespace en
{
	Config* Config::Get()
	{
		static std::unique_ptr<en::Config> instance = std::make_unique<en::Config>();
		return instance.get();
	}


	Config::Config()
		: m_isDirty(false)
		, m_renderingAPI(platform::RenderingAPI::RenderingAPI_Count)
	{
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
		string argString; // The full list of all command line args received		
		for (int i = 1; i < argc; i++)
		{
			std::string currentToken = argv[i];

			// Append the current token to our argument string:
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

		// Store the received command line string
		SetValue(ConfigKeys::k_commandLineArgsValueKey, argString, Config::SettingType::Runtime);

		// Process the key/value pairs:
		for (size_t i = 0; i < keysValues.size(); i++)
		{
			if (keysValues[i].HasValue())
			{
				bool isNumericValue = true;
				try
				{
					int numericValue = std::stoi(keysValues[i].m_value);
					SetValue(keysValues[i].m_key, numericValue, Config::SettingType::Runtime);
				}
				catch (std::invalid_argument)
				{
					isNumericValue = false;
				}
				if (!isNumericValue)
				{
					SetValue(keysValues[i].m_key, keysValues[i].m_value, Config::SettingType::Runtime);
				}
			}
			else
			{
				// If no value was provided with a key, just set it as a boolean flag
				SetValue(keysValues[i].m_key, true, Config::SettingType::Runtime);
			}
		}

		// Post-processing:
		if (KeyExists(en::ConfigKeys::k_sceneCmdLineArg))
		{
			std::string const& sceneNameParam = GetValue<std::string>(en::ConfigKeys::k_sceneCmdLineArg);

			// From param of the form "Scene\Folder\Names\sceneFile.extension", we extract:

			// sceneFilePath == ".\Scenes\Scene\Folder\Names\sceneFile.extension":
			const string sceneFilePath = en::ConfigKeys::k_scenesDirNameKey + sceneNameParam; // k_scenesDirNameKey = ".\Scenes\"
			SetValue(en::ConfigKeys::k_sceneFilePathKey, sceneFilePath, Config::SettingType::Runtime);

			// sceneRootPath == ".\Scenes\Scene\Folder\Names\":
			const size_t lastSlash = sceneFilePath.find_last_of("\\");
			const string sceneRootPath = sceneFilePath.substr(0, lastSlash) + "\\";
			SetValue(en::ConfigKeys::k_sceneRootPathKey, sceneRootPath, Config::SettingType::Runtime);

			// sceneName == "sceneFile"
			const string filenameAndExt = sceneFilePath.substr(lastSlash + 1, sceneFilePath.size() - lastSlash);
			const size_t extensionPeriod = filenameAndExt.find_last_of(".");
			const string sceneName = filenameAndExt.substr(0, extensionPeriod);
			SetValue(en::ConfigKeys::k_sceneNameKey, sceneName, Config::SettingType::Runtime);

			// sceneIBLPath == ".\Scenes\SceneFolderName\IBL\ibl.hdr"
			const string sceneIBLPath = sceneRootPath + "IBL\\ibl.hdr";
			SetValue(en::ConfigKeys::k_sceneIBLPathKey, sceneIBLPath, Config::SettingType::Runtime);
		}

		if (KeyExists(en::ConfigKeys::k_platformCmdLineArg))
		{
			std::string const& platformParam = GetValue<std::string>(en::ConfigKeys::k_platformCmdLineArg);

			bool platformValueIsValid = false;
			if (platformParam.find("opengl") != string::npos)
			{
				m_renderingAPI = platform::RenderingAPI::OpenGL;
				platformValueIsValid = true;
			}
			else if (platformParam.find("dx12") != string::npos)
			{
				m_renderingAPI = platform::RenderingAPI::DX12;
				platformValueIsValid = true;
			}

			if (platformValueIsValid)
			{
				SetValue(en::ConfigKeys::k_platformCmdLineArg, platformParam, SettingType::APISpecific);
			}
		}

		// We don't count command line arg entries as dirtying the config
		m_isDirty = false;
	}


	void Config::LoadConfigFile()
	{
		LOG("Loading %s...", en::ConfigKeys::k_configFileName);

		ifstream file;
		file.open(std::format("{}{}", en::ConfigKeys::k_configDirName, en::ConfigKeys::k_configFileName).c_str());

		// If no config is found, create one:
		const bool foundExistingConfig = file.is_open();
		if (!foundExistingConfig)
		{
			LOG_WARNING("No config.cfg file found! Attempting to create a default version");
			m_isDirty = true;
			SaveConfigFile();
		}

		// Process the config file:
		string line;
		bool foundInvalidString = false;
		while (file.good())
		{
			// Handle malformed strings from previous iteration:
			if (foundInvalidString == true)
			{
				LOG_WARNING("Ignoring invalid command in config.cfg:\n%s", line);
				foundInvalidString = false;
			}

			// Get the next line:
			getline(file, line);

			// Replace whitespace with single spaces:
			std::regex tabMatch("([\\s])+");
			string cleanLine = std::regex_replace(line, tabMatch, " ");

			// Skip empty or near-empty lines:
			if (cleanLine.find_first_not_of(" \t\n") == string::npos || cleanLine.length() <= 2)
			{
				continue;
			}

			// Remove single leading space, if it exists:
			if (cleanLine.at(0) == ' ')
			{
				cleanLine = cleanLine.substr(1, string::npos);
			}

			// Remove comments:
			size_t commentStart = cleanLine.find_first_of("#");
			if (commentStart != string::npos)
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
			string command = cleanLine.substr(0, firstSpace);

			// Remove the command from the head of the string:
			cleanLine = cleanLine.substr(firstSpace + 1, string::npos);

			// Extract the variable property name:
			firstSpace = cleanLine.find_first_of(" \t\n", 1);
			string property = cleanLine.substr(0, firstSpace);

			// Remove the property from the head of the string:
			cleanLine = cleanLine.substr(firstSpace + 1, string::npos);

			// Clean up the value string:
			string value = cleanLine;

			// Remove quotation marks from value string:
			bool isString = false;
			if (value.find("\"") != string::npos)
			{
				isString = true;
				std::regex quoteMatch("([\\\"])+");
				value = std::regex_replace(value, quoteMatch, "");
			}

			// Update config hashtables. We set all SettingsType as common, to ensure otherwise API-specific settings
			// will be written to disk
			if (command == SET_CMD)
			{
				// Strings:
				if (isString)
				{
					TrySetValue(property, string(value), SettingType::Common);
				}
				else
				{
					// Booleans:
					string boolString = ToLowerCase(value);
					if (boolString == TRUE_STRING)
					{
						TrySetValue(property, true, SettingType::Common);
						continue;
					}
					else if (boolString == FALSE_STRING)
					{
						TrySetValue(property, false, SettingType::Common);
						continue;
					}

					// TODO: Handle strings without "quotations" -> If it doesn't contain numbers, assume it's a string

					// Numeric values: Try and cast as an int, and fallback to a float if it fails
					size_t position = 0;
					int intResult = std::stoi(value, &position);

					// Ints:
					if (position == value.length())
					{
						TrySetValue(property, intResult, SettingType::Common);
					}
					else // Floats:
					{
						float floatResult = std::stof(value);
						TrySetValue(property, floatResult, SettingType::Common);
					}
				}
			}
			else if (command == "bind")
			{
				if (isString)
				{
					TrySetValue(property, string(value), SettingType::Common);
				}
				else
				{
					// Assume bound values are single chars, for now. Might need to rework this to bind more complex keys
					TrySetValue(property, (char)value[0], SettingType::Common);
				}
			}
			else
			{
				foundInvalidString = true;
				continue;
			}
		}

		// Handle final malformed string:
		if (foundInvalidString == true)
		{
			LOG_WARNING("Ignoring invalid command in config.cfg:\n%s", line);
		}

		// We don't count existing config.cfg entries as dirtying the config
		m_isDirty = !foundExistingConfig;

		// Now the config has been loaded, populate any remaining entries with default values
		InitializeOSValues();
		InitializeDefaultValues();
		SetAPIDefaults();
		SetRuntimeDefaults();

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

		TrySetValue(ConfigKeys::k_documentsFolderPathKey, documentFolderPath, SettingType::Runtime);
	}


	void Config::InitializeDefaultValues()
	{
		bool markDirty = false;

		if (m_renderingAPI == platform::RenderingAPI::RenderingAPI_Count)
		{
			m_renderingAPI = platform::RenderingAPI::DX12;
		}
		
		// Window:
		markDirty |= TrySetValue("windowTitle",				std::string("Saber Engine"),	SettingType::Common);
		markDirty |= TrySetValue(ConfigKeys::k_windowWidthKey,	1920,		SettingType::Common);
		markDirty |= TrySetValue(ConfigKeys::k_windowHeightKey,	1080,		SettingType::Common);

		// System config:
		markDirty |= TrySetValue("vsync",					true,				SettingType::Common);

		// Texture dimensions:
		markDirty |= TrySetValue(en::ConfigKeys::k_defaultShadowMapResolution,		2048,	SettingType::Common);
		markDirty |= TrySetValue(en::ConfigKeys::k_defaultShadowCubeMapResolution,	512,	SettingType::Common);

		// Camera defaults:
		markDirty |= TrySetValue("defaultyFOV",				1.570796f,			SettingType::Common);
		markDirty |= TrySetValue("defaultNear",				1.0f,				SettingType::Common);
		markDirty |= TrySetValue("defaultFar",				100.0f,				SettingType::Common);

		// Input parameters:
		markDirty |= TrySetValue(en::ConfigKeys::k_mousePitchSensitivity,	0.5f,	SettingType::Common);
		markDirty |= TrySetValue(en::ConfigKeys::k_mouseYawSensitivity,		0.5f,	SettingType::Common);
		markDirty |= TrySetValue(en::ConfigKeys::k_sprintSpeedModifier,		2.0f,	SettingType::Common);

		// Scene data:
		markDirty |= TrySetValue(en::ConfigKeys::k_defaultIBLPathKey,	"Assets\\DefaultIBL\\ibl.hdr",	SettingType::Common);

		// Key bindings:
		//--------------
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Forward),	'w',			SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Backward),	's',			SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Left),		'a',			SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Right),	'd',			SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Up),		"Space",		SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Down),		"Left Shift",	SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Sprint),	"Left Ctrl",	SettingType::Common);

		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Console),	"Grave",		SettingType::Common); // The "grave accent"/tilde key: `
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_VSync),	'v',			SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputButton_Quit),		"Escape",		SettingType::Common);

		// Mouse bindings:
		markDirty |= TrySetValue(ENUM_TO_STR(InputMouse_Left),	ENUM_TO_STR(InputMouse_Left),	SettingType::Common);
		markDirty |= TrySetValue(ENUM_TO_STR(InputMouse_Right),	ENUM_TO_STR(InputMouse_Right),	SettingType::Common);

		m_isDirty |= markDirty;
	}


	void en::Config::SetAPIDefaults()
	{
		// We only set these defaults if they're not specified in the (now already loaded) config file. This allows the
		// config to override these values, if required. We also tag these keys as API-specific (but if they're found in
		// the config, they're loaded as Common, ensuring they're be saved back out.
		// Note: Strings must be passed as string objects (not CStrings)
		auto TryInsertDefault = [&](std::string const& key, auto const& value)
		{
			TrySetValue(key, value, SettingType::APISpecific);
		};		

		platform::RenderingAPI const& api = GetRenderingAPI();
		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			// Shaders:
			TryInsertDefault(en::ConfigKeys::k_shaderDirectoryKey,	std::string(".\\Shaders\\GLSL\\"));
			// Note: OpenGL only supports double-buffering, so we don't add a k_numBackbuffersKey entry
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			TryInsertDefault(en::ConfigKeys::k_shaderDirectoryKey, std::string(".\\Shaders\\HLSL\\"));
			TryInsertDefault(en::ConfigKeys::k_numBackbuffersKey, 3);
		}
		break;
		default:
			LOG_ERROR("Config failed to set API Defaults! "
				"Does the config.cfg file contain a 'set platform \"<API>\" command for a supported API?");

			throw std::runtime_error("Invalid Rendering API set, cannot set API defaults");
		}
	}


	void Config::SetRuntimeDefaults()
	{
		auto TryInsertRuntimeValue = [&](std::string const& key, auto const& value)
		{
			TrySetValue(key, value, SettingType::Runtime);
		};

		// Debug:
		TryInsertRuntimeValue(ConfigKeys::k_debugLevelCmdLineArg, 0);

		// Quality settings:
		TryInsertRuntimeValue(en::ConfigKeys::k_brdfLUTWidthHeight,		1024);
		TryInsertRuntimeValue(en::ConfigKeys::k_iemTexWidthHeight,		512);
		TryInsertRuntimeValue(en::ConfigKeys::k_iemNumSamples,			4096);
		TryInsertRuntimeValue(en::ConfigKeys::k_pmremTexWidthHeight,	1024);
		TryInsertRuntimeValue(en::ConfigKeys::k_pmremNumSamples,		4096);

		// Shadow map defaults:
		TryInsertRuntimeValue(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias,	0.001f);
		TryInsertRuntimeValue(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias,	0.005f);
		TryInsertRuntimeValue(en::ConfigKeys::k_defaultPointLightMinShadowBias,			0.01f);
		TryInsertRuntimeValue(en::ConfigKeys::k_defaultPointLightMaxShadowBias,			0.05f);
	}
	

	bool Config::KeyExists(std::string const& valueName) const
	{
		auto const& result = m_configValues.find(valueName);
		return result != m_configValues.end();
	}


	string Config::GetValueAsString(const string& valueName) const
	{
		auto const& result = m_configValues.find(valueName);
		string returnVal = "";
		if (result != m_configValues.end())
		{
			try
			{
				if (result->second.first.type() == typeid(string))
				{
					returnVal = any_cast<string>(result->second.first);
				}
				else if (result->second.first.type() == typeid(char const*))
				{
					returnVal = string(any_cast<char const*>(result->second.first));
				}
				else if (result->second.first.type() == typeid(float))
				{
					float configValue = any_cast<float>(result->second.first);
					returnVal = to_string(configValue);
				}
				else if (result->second.first.type() == typeid(int))
				{
					int configValue = any_cast<int>(result->second.first);
					returnVal = to_string(configValue);
				}
				else if (result->second.first.type() == typeid(char))
				{
					char configValue = any_cast<char>(result->second.first);
					returnVal = string(1, configValue); // Construct a string with 1 element
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
			LOG_ERROR("Config key \"%s\" does not exist\n", valueName.c_str());
		}

		return returnVal;
	}


	std::wstring Config::GetValueAsWString(const std::string& valueName) const
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

		// Create the .\config\ directory, if none exists
		std::filesystem::path configPath = en::ConfigKeys::k_configDirName;
		if (!std::filesystem::exists(configPath))
		{
			LOG("Creating .\\config\\ directory");

			std::filesystem::create_directory(configPath);
		}

		// Build a list of the strings we plan to write, so we can sort them
		struct ConfigEntry
		{
			std::string m_cmdPrefix; // SET_CMD, BIND_CMD
			std::string m_key;
			std::string m_value;
		};
		std::vector<ConfigEntry> configEntries;
		configEntries.reserve(m_configValues.size());

		for (std::pair<string, std::pair<any, SettingType>> currentElement : m_configValues)
		{
			if (currentElement.second.second == SettingType::APISpecific || 
				currentElement.second.second == SettingType::Runtime)
			{
				continue;	// Skip API-specific settings
			}

			if (currentElement.second.first.type() == typeid(string) && currentElement.first.find("Input") == string::npos)
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = SET_CMD,
					.m_key = currentElement.first,
					.m_value = PropertyToConfigString(any_cast<string>(currentElement.second.first))});
			}
			else if (currentElement.second.first.type() == typeid(char const*) && currentElement.first.find("Input") == string::npos)
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = SET_CMD,
					.m_key = currentElement.first,
					.m_value = PropertyToConfigString(any_cast<char const*>(currentElement.second.first)) });
			}
			else if (currentElement.second.first.type() == typeid(float))
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = SET_CMD,
					.m_key = currentElement.first,
					.m_value = PropertyToConfigString(any_cast<float>(currentElement.second.first)) });
			}
			else if (currentElement.second.first.type() == typeid(int))
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = SET_CMD,
					.m_key = currentElement.first,
					.m_value = PropertyToConfigString(any_cast<int>(currentElement.second.first)) });
			}
			else if (currentElement.second.first.type() == typeid(bool))
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = SET_CMD,
					.m_key = currentElement.first,
					.m_value = PropertyToConfigString(any_cast<bool>(currentElement.second.first)) });
			}
			else if (currentElement.second.first.type() == typeid(char))
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = BIND_CMD,
					.m_key = currentElement.first,
					.m_value = PropertyToConfigString(any_cast<char>(currentElement.second.first)) });
			}
			else if (currentElement.second.first.type() == typeid(string) && currentElement.first.find("Input") != string::npos)
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = BIND_CMD,
					.m_key = currentElement.first,
					.m_value = PropertyToConfigString(any_cast<string>(currentElement.second.first)) });
			}
			else if (currentElement.second.first.type() == typeid(char const*) && currentElement.first.find("Input") != string::npos)
			{
				configEntries.emplace_back(ConfigEntry{
					.m_cmdPrefix = BIND_CMD,
					.m_key = currentElement.first,
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
			return cmpResult < 0;});

		// Write our config to disk:
		std::ofstream config_ofstream(
			std::format("{}{}", en::ConfigKeys::k_configDirName, en::ConfigKeys::k_configFileName));
		config_ofstream << "# SaberEngine config.cfg file:\n";

		for (ConfigEntry const& currentEntry : configEntries)
		{
			config_ofstream << currentEntry.m_cmdPrefix << " " << currentEntry.m_key << currentEntry.m_value;
		}

		m_isDirty = false;
	}


	float Config::GetWindowAspectRatio() const
	{
		return (float)(GetValue<int>("windowXRes")) / (float)(GetValue<int>("windowYRes"));
	}


	inline const platform::RenderingAPI Config::GetRenderingAPI() const
	{
		return m_renderingAPI;
	}


	inline std::string Config::PropertyToConfigString(std::string property)
	{
		return " \"" + property + "\"\n";
	}


	inline std::string Config::PropertyToConfigString(char const* property)
	{
		return " \"" + std::string(property) + "\"\n";
	}


	inline std::string Config::PropertyToConfigString(float property)
	{
		return " " + std::to_string(property) + "\n";
	}


	inline std::string Config::PropertyToConfigString(int property)
	{
		return " " + std::to_string(property) + "\n";
	}


	inline std::string Config::PropertyToConfigString(char property)
	{
		return std::string(" ") + property + std::string("\n");
	}


	inline string Config::PropertyToConfigString(bool property)
	{
		return string(" ") + (property ? TRUE_STRING : FALSE_STRING) + string("\n");
	}
}