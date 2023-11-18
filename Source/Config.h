// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "ConfigKeys.h"
#include "Platform.h"
#include "DebugConfiguration.h"


namespace en
{
	class Config
	{
	public: 
		enum class SettingType
		{
			Common,				// Platform-agnostic value. Saved to disk.
			APISpecific,		// API-specific value: Not saved to disk (unless found in config at load time)
			Runtime,			// Platform-agnostic value populated at runtime. Not saved to disk.
			SettingType_Count
		};


	public: 
		static Config* Get(); // Singleton functionality


	public:
		Config();

		void ProcessCommandLineArgs(int argc, char** argv);

		void LoadConfigFile(); // Load the config.cfg file

		void SaveConfigFile(); // Save config.cfg to disk

		template<typename T>
		T GetValue(const std::string& valueName) const;

		template<typename T>
		bool TryGetValue(std::string const& valueName, T& value) const;

		bool ValueExists(std::string const& valueName) const;

		std::string GetValueAsString(const std::string& valueName) const;
		std::wstring GetValueAsWString(const std::string& valueName) const;
		
		// Specific configuration retrieval:
		/**********************************/
		float GetWindowAspectRatio() const; // Compute the aspect ratio: width / height

		const platform::RenderingAPI GetRenderingAPI() const;


	private:
		template<typename T>
		void SetValue(const std::string& valueName, T value, SettingType settingType = SettingType::Common);

		template<typename T>
		void SetValue(char const* valueName, T value, SettingType settingType = SettingType::Common);

		// Set a new config value, IFF it doesn't already exist. Returns true if the value was set
		template<typename T>
		bool TrySetValue(const std::string& valueName, T value, SettingType settingType = SettingType::Common);

		template<typename T>
		bool TrySetValue(char const* valueName, T value, SettingType settingType = SettingType::Common);


	private:
		void InitializeDefaultValues(); // Initialize any unpopulated configuration values with defaults
		void SetAPIDefaults();
		void SetRuntimeDefaults(); // Platform-agnostic defaults not loaded/saved to disk


	private:
		std::unordered_map<std::string, std::pair<std::any, SettingType>> m_configValues;	// The config parameter/value map
		bool m_isDirty; // Marks whether we need to save the config file or not

		platform::RenderingAPI m_renderingAPI;


	private: // Helper functions:
		std::string PropertyToConfigString(std::string property);
		std::string PropertyToConfigString(char const* property);
		std::string PropertyToConfigString(float property);
		std::string PropertyToConfigString(int property);
		std::string PropertyToConfigString(char property);
		std::string PropertyToConfigString(bool property);
	};


	// Get a config value, by type
	template<typename T>
	T Config::GetValue(const std::string& valueName) const
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
				// TODO: The SEAssertF macro should take variadic args
				LOG_ERROR("bad_any_cast exception thrown: Invalid type requested from Config\n%s", e.what());
				SEAssertF("bad_any_cast exception thrown: Invalid type requested from Config");
			}
		}
		else
		{
			LOG_ERROR("Invalid type requested from Config");
			SEAssertF("Invalid type requested from Config");
			throw std::runtime_error("Config key does not exist");
		}

		return returnVal;
	}


	template<typename T>
	bool Config::TryGetValue(std::string const& valueName, T& value) const
	{
		if (!ValueExists(valueName))
		{
			return false;
		}

		value = GetValue<T>(valueName);
		return true;
	}


	// Set a config value
	// Note: Strings must be explicitely defined as a string("value")
	template<typename T>
	void Config::SetValue(const std::string& valueName, T value, SettingType settingType /*= SettingType::Common*/)
	{
		m_configValues[valueName] = { value, settingType };
		if (settingType == SettingType::Common)
		{
			m_isDirty = true;
		}
	}

	template<typename T>
	inline void Config::SetValue(char const* valueName, T value, SettingType settingType /*= SettingType::Common*/)
	{
		SetValue(std::string(valueName), value, settingType);
	}

	template<typename T>
	bool Config::TrySetValue(const std::string& valueName, T value, SettingType settingType /*= SettingType::Common*/)
	{
		if (ValueExists(valueName))
		{
			return false;
		}
		
		SetValue(valueName, value, settingType);
		return true;
	}

	template<typename T>
	inline bool Config::TrySetValue(char const* valueName, T value, SettingType settingType /*= SettingType::Common*/)
	{
		return TrySetValue(std::string(valueName), value, settingType);
	}
}


