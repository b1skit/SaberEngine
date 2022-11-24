#include "Config.h"
#include "DebugConfiguration.h"
#include "KeyConfiguration.h"

#include <fstream>
#include <filesystem>
#include <regex>
#include <stdexcept>
#include <type_traits>

using std::ifstream;
using std::any_cast;
using std::string;
using std::to_string;
using std::unordered_map;
using std::any;


// Default true/false strings. We convert config values to lowercase and compare against these
#define TRUE_STRING		"true"
#define FALSE_STRING	"false"

// Command strings: End with a space to maintain formatting
#define SET_CMD		"set "		// Set a value
#define BIND_CMD	"bind "		// Bind a key


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


	void Config::InitializeDefaultValues()
	{
		// Define the default values in unordered_map, to simplify (de)serialization.
		// Note: String values must be explicitely defined as string objects
		m_configValues =
		{
			{"platform",							{string("opengl"), SettingType::Common}},

			{"windowTitle",							{string("Saber Engine"), SettingType::Common}},
			{"windowXRes",							{1920, SettingType::Common}},
			{"windowYRes",							{1080, SettingType::Common}},

			{"vsync",								{true, SettingType::Common}},

			// Camera defaults:
			{"defaultyFOV",							{1.570796f, SettingType::Common}},
			{"defaultNear",							{1.0f,		SettingType::Common}},
			{"defaultFar",							{100.0f,	SettingType::Common}},
			{"defaultExposure",						{1.0f,		SettingType::Common}},

			// Input parameters:
			{"mousePitchSensitivity",				{0.00005f,	SettingType::Common}},
			{"mouseYawSensitivity",					{0.00005f,	SettingType::Common}},
			{"sprintSpeedModifier",					{2.0f,		SettingType::Common}},

			// SceneData config root path: All assets stored here
			{"scenesRoot",							{string("..\\Scenes\\"), SettingType::Common}},	
			{"defaultIBLPath",						{string("..\\Assets\\DefaultIBL\\ibl.hdr"), SettingType::Common}},

			// Key bindings:
			//--------------
			{ENUM_TO_STR(InputButton_Forward),	{'w', SettingType::Common}},
			{ENUM_TO_STR(InputButton_Backward),	{'s', SettingType::Common}},
			{ENUM_TO_STR(InputButton_Left),		{'a', SettingType::Common}},
			{ENUM_TO_STR(InputButton_Right),	{'d', SettingType::Common}},
			{ENUM_TO_STR(InputButton_Up),		{"Space", SettingType::Common}},
			{ENUM_TO_STR(InputButton_Down),		{"Left Shift", SettingType::Common}},
			{ENUM_TO_STR(InputButton_Sprint),	{"Left Ctrl", SettingType::Common}},

			{ENUM_TO_STR(InputButton_Console),	{'`', SettingType::Common}}, // The "grave accent"/tilde key
			{ENUM_TO_STR(InputButton_Quit),		{"Escape", SettingType::Common}},

			// Mouse bindings:
			{ENUM_TO_STR(InputMouse_Left),		{string(ENUM_TO_STR(InputMouse_Left)), SettingType::Common}},
			{ENUM_TO_STR(InputMouse_Right),		{string(ENUM_TO_STR(InputMouse_Right)), SettingType::Common}},

		};

		m_isDirty = true;
	}


	void en::Config::SetAPIDefaults()
	{
		// We only set these defaults if they're not specified in the (now already loaded) config file. This allows the
		// config to override these values, if required. We also tag these keys as API-specific (but if they're found in
		// the config, they're loaded as Common, ensuring they're be saved back out.
		// Note: Strings must be passed as string objects (not CStrings)
		auto TryInsertDefault = [&](std::string const& key, auto const& value)
		{
			auto const& result = m_configValues.find(key);
			if (result == m_configValues.end())
			{
				SetValue(key, value, SettingType::APISpecific);
			}
		};


		platform::RenderingAPI const& api = GetRenderingAPI();
		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			// Shader:
			TryInsertDefault("shaderDirectory",						std::string(".\\Shaders\\glsl\\"));

			// Depth map rendering:
			TryInsertDefault("depthShaderName",						std::string("depthShader"));
			TryInsertDefault("cubeDepthShaderName",					std::string("cubeDepthShader"));

			// Deferred rendering:
			TryInsertDefault("gBufferFillShaderName",				std::string("gBufferFillShader"));
			TryInsertDefault("deferredAmbientLightShaderName",		std::string("deferredAmbientLightShader"));
			TryInsertDefault("deferredKeylightShaderName",			std::string("deferredKeyLightShader"));
			TryInsertDefault("deferredPointLightShaderName",		std::string("deferredPointLightShader"));
			TryInsertDefault("skyboxShaderName",					std::string("skyboxShader"));
			TryInsertDefault("equilinearToCubemapBlitShaderName",	std::string("equilinearToCubemapBlitShader"));
			TryInsertDefault("BRDFIntegrationMapShaderName",		std::string("BRDFIntegrationMapShader"));
			TryInsertDefault("blitShaderName",						std::string("blitShader"));
			TryInsertDefault("blurShaderName",						std::string("blurShader"));
			TryInsertDefault("toneMapShader",						std::string("toneMapShader"));

			// Multiplier used to scale [0,1] emissive values when writing to GBuffer, so they'll bloom
			TryInsertDefault("defaultSceneEmissiveIntensity",		2.0f);

			// Quality settings:
			TryInsertDefault("numIEMSamples",	20000);	// Number of samples to use when generating IBL IEM texture
			TryInsertDefault("numPMREMSamples",	4096);	// Number of samples to use when generating IBL PMREM texture

			TryInsertDefault("defaultIBLPath",	std::string("IBL\\ibl.hdr"));

			// Shadow map defaults:
			TryInsertDefault("defaultMinShadowBias",	0.01f);
			TryInsertDefault("defaultMaxShadowBias",	0.05f);

			// Texture dimensions:
			TryInsertDefault("defaultShadowMapRes",		(uint32_t)2048);
			TryInsertDefault("defaultShadowCubeMapRes",	(uint32_t)512);

		}
			break;

		case platform::RenderingAPI::DX12:
		{
			// TBC...
		}
		break;

		default:
			LOG_ERROR("Config failed to set API Defaults! "
				"Does the config.cfg file contain a 'set platform \"<API>\" command for a supported API?");

			throw std::runtime_error("Invalid Rendering API set, cannot set API defaults");
		}
		
	}


	// Get a config value, by type
	template<typename T>
	T Config::GetValue(const string& valueName) const
	{
		auto const& result = m_configValues.find(valueName);
		T returnVal{};
		if (result != m_configValues.end())
		{
			try
			{
				returnVal = any_cast<T>(result->second.first);
			}
			catch (const std::bad_any_cast& e)
			{
				LOG_ERROR("bad_any_cast exception thrown: Invalid type requested from Config\n%s", e.what());
			}
		}
		else
		{
			throw std::runtime_error("Config key does not exist");
		}

		return returnVal;
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template string Config::GetValue<string>(const string& valueName) const;
	template float Config::GetValue<float>(const string& valueName) const;
	template int32_t Config::GetValue<int32_t>(const string& valueName) const;
	template uint32_t Config::GetValue<uint32_t>(const string& valueName) const;
	template bool Config::GetValue<bool>(const string& valueName) const;
	template char Config::GetValue<char>(const string& valueName) const;


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


	// Set a config value
	// Note: Strings must be explicitely defined as a string("value")
	template<typename T>
	void Config::SetValue(const string& valueName, T value, SettingType settingType /*= SettingType::Common*/) 
	{
		m_configValues[valueName] = {value, settingType};
		m_isDirty = true;
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template void Config::SetValue<string>(const string& valueName, string value, SettingType settingType);
	template void Config::SetValue<float>(const string& valueName, float value, SettingType settingType);
	template void Config::SetValue<int>(const string& valueName, int value, SettingType settingType);
	template void Config::SetValue<bool>(const string& valueName, bool value, SettingType settingType);
	template void Config::SetValue<char>(const string& valueName, char value, SettingType settingType);



	// Constructor
	Config::Config() :
		m_isDirty{ true },
		m_renderingAPI{ platform::RenderingAPI::RenderingAPI_Count }
	{
		// Populate the config hash table with initial values
		InitializeDefaultValues();

		// Load config.cfg file
		LoadConfig();

		// Update specific efficiency functions:
		std::string platform = GetValueAsString("platform");
		if (platform == "opengl")
		{
			m_renderingAPI = platform::RenderingAPI::OpenGL;
		}
		else if (platform == "dx12")
		{
			m_renderingAPI = platform::RenderingAPI::DX12;
		}
		else
		{
			LOG_ERROR("Config failed to set valid rendering API! "
				"Does the config contain a 'set platform \"<API>\" command? e.g:\n"
				"set platform \"opengl\"\n"
				"set platform \"dx12\"\n"
				"Defaulting to OpenGL...");
			m_renderingAPI = platform::RenderingAPI::OpenGL;
		}

		// Set API-specific defaults:
		SetAPIDefaults();
	}


	void Config::LoadConfig()
	{
		LOG("Loading %s...", m_configFilename.c_str());

		ifstream file;
		file.open((m_configDir + m_configFilename).c_str());

		// If no config is found, create one:
		if (!file.is_open())
		{
			LOG_WARNING("No config.cfg file found! Attempting to create a default version");

			m_isDirty = true;

			SaveConfig();

			return;
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
			if (cleanLine.find_first_not_of(" \t\n") == string::npos || cleanLine.length() <= 2) // TODO: Choose a more meaningful minimum line length
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
			if (command == "set")
			{
				// Strings:
				if (isString)
				{
					m_configValues[property] = { string(value), SettingType::Common };
				}
				else
				{
					// Booleans:
					string boolString = ToLowerCase(value);
					if (boolString == TRUE_STRING)
					{
						m_configValues[property] = { true, SettingType::Common };
						continue;
					}
					else if (boolString == FALSE_STRING)
					{
						m_configValues[property] = { false, SettingType::Common	};
						continue;
					}

					// TODO: Handle strings without "quotations" -> If it doesn't contain numbers, assume it's a string

					// Numeric values: Try and cast as an int, and fallback to a float if it fails
					size_t position = 0;
					int intResult = std::stoi(value, &position);

					// Ints:
					if (position == value.length())
					{
						m_configValues[property] = {intResult, SettingType::Common };
					}
					else // Floats:
					{
						float floatResult = std::stof(value);
						m_configValues[property] = {floatResult, SettingType::Common };
					}
				}

			}
			else if (command == "bind")
			{
				if (isString)
				{
					m_configValues[property] = {string(value), SettingType::Common };
				}
				else
				{
					// Assume bound values are single chars, for now. Might need to rework this to bind more complex keys
					m_configValues[property] = {(char)value[0], SettingType::Common }; 
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

		m_isDirty = false;

		LOG("Done!");
	}


	void Config::SaveConfig()
	{
		if (m_isDirty == false)
		{
			LOG("SaveConfig called, but config has not changed. Returning without modifying file on disk");
			return;
		}

		// Create the .\config\ directory, if none exists
		std::filesystem::path configPath = m_configDir;
		if (!std::filesystem::exists(configPath))
		{
			LOG("Creating .\\config\\ directory");

			std::filesystem::create_directory(configPath);
		}

		// Write our config to disk:
		std::ofstream config_ofstream(m_configDir + m_configFilename);
		config_ofstream << "# SaberEngine config.cfg file:\n";


		// Output each value, by type:
		for (std::pair<string, std::pair<any, SettingType>> currentElement : m_configValues)
		{
			if (currentElement.second.second == SettingType::APISpecific || currentElement.second.second == SettingType::Runtime)
			{
				continue;	// Skip API-specific settings
			}

			if (currentElement.second.first.type() == typeid(string) && currentElement.first.find("INPUT") == string::npos)
			{
				config_ofstream << SET_CMD << currentElement.first << PropertyToConfigString(any_cast<string>(currentElement.second.first));
			}
			else if (currentElement.second.first.type() == typeid(float))
			{
				config_ofstream << SET_CMD << currentElement.first << PropertyToConfigString(any_cast<float>(currentElement.second.first));
			}
			else if (currentElement.second.first.type() == typeid(int))
			{
				config_ofstream << SET_CMD << currentElement.first << PropertyToConfigString(any_cast<int>(currentElement.second.first));
			}
			else if (currentElement.second.first.type() == typeid(bool))
			{
				config_ofstream << SET_CMD << currentElement.first << PropertyToConfigString(any_cast<bool>(currentElement.second.first));
			}
			else if (currentElement.second.first.type() == typeid(char))
			{
				config_ofstream << BIND_CMD << currentElement.first << PropertyToConfigString(any_cast<char>(currentElement.second.first));
			}
			else if (currentElement.second.first.type() == typeid(string) && currentElement.first.find("INPUT") != string::npos)
			{
				config_ofstream << BIND_CMD << currentElement.first << PropertyToConfigString(any_cast<string>(currentElement.second.first));
			}
			else
			{
				LOG_ERROR("Cannot write unsupported type to config");
			}
		}
		

		m_isDirty = false;
	}


	// Note: We inline this here, as it depends on macros defined in KeyConfiguration.h
	inline string Config::PropertyToConfigString(bool property) { return string(" ") + (property == true ? TRUE_STRING : FALSE_STRING) + string("\n"); }
}