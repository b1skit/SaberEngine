// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "Logger.h"

#include "Definitions/ConfigKeys.h"

#include "Util/CHashKey.h"


namespace core
{
	class Config final
	{
	public: 
		enum class SettingType
		{
			Serialized,			// Saved to disk.
			Runtime,			// Populated at runtime. Not saved to disk.

			SettingType_Count
		};


	public: 
		static Config* Get(); // Singleton functionality


	public:
		Config();
		~Config();

		Config(Config&&) noexcept = default;
		Config& operator=(Config&&) noexcept = default;

	public:
		void SetCommandLineArgs(int argc, char** argv);
		void ProcessCommandLineArgs();

		void LoadConfigFile(); // Load the config file
		void SaveConfigFile(); // Save config file to disk


	public:
		template<typename T>
		T GetValue(util::CHashKey const&) const;

		template<typename T>
		bool TryGetValue(util::CHashKey const&, T& valueOut) const;

		bool KeyExists(util::CHashKey const&) const;

		std::string GetValueAsString(util::CHashKey const&) const;
		std::wstring GetValueAsWString(util::CHashKey const&) const;
	

	public:
		template<typename T>
		void SetValue(util::CHashKey const&, T const& value, SettingType = SettingType::Serialized);

		// Set a new config value, IFF it doesn't already exist. Returns true if the value was set
		template<typename T>
		bool TrySetValue(util::CHashKey const&, T const& value, SettingType = SettingType::Serialized);


	private:
		void InitializeOSValues(); // Values loaded from the OS. Not saved to disk.
		void InitializeDefaultValues(); // Initialize any unpopulated configuration values with defaults
		void SetRuntimeDefaults(); // Platform-agnostic defaults not loaded/saved to disk
		

	private:
		using ConfigValue = std::variant<bool, int, float, char, char const*, std::string>;
		std::unordered_map<util::CHashKey, std::pair<ConfigValue, SettingType>> m_configValues;
		mutable std::shared_mutex m_configValuesMutex;
		bool m_isDirty; // Marks whether we need to save the config file or not

		// Command line arguments:
		int m_argc;
		char** m_argv;


	private: // Helper functions:
		template<typename T>
		std::string PropertyToConfigString(T property);

		template<>
		std::string PropertyToConfigString(std::string const&);

		template<>
		std::string PropertyToConfigString(char const*);

		template<>
		std::string PropertyToConfigString(char);

		template<>
		std::string PropertyToConfigString(bool);

		template<>
		std::string PropertyToConfigString(float);


	private:
		static constexpr char const* k_trueString = "true";
		static constexpr char const* k_falseString = "false";
		static constexpr char const* k_setCmd = "set"; // Set a value
		static constexpr char const* k_bindCmd = "bind"; // Bind a key


	private: // No copying allowed
		Config(Config const&) = delete;
		Config& operator=(Config const&) = delete;
	};


	// Get a config value, by type
	template<typename T>
	T Config::GetValue(util::CHashKey const& key) const
	{
		T returnVal{};
		{
			std::shared_lock<std::shared_mutex> readLock(m_configValuesMutex);

			bool foundValue = false;
			auto const& result = m_configValues.find(key);
			if (result != m_configValues.end())
			{
				if (T const* val = std::get_if<T>(&result->second.first))
				{
					returnVal = *val;
					foundValue = true;
				}
			}
			
			if (!foundValue)
			{
				LOG_ERROR("Config::GetValue: Key does not exist");
			}
		}
		return returnVal;
	}


	template<typename T>
	bool Config::TryGetValue(util::CHashKey const& key, T& valueOut) const
	{
		if (!KeyExists(key))
		{
			return false;
		}

		valueOut = GetValue<T>(key);
		return true;
	}


	// Set a config value
	// Note: Strings must be explicitely defined as a string("value")
	template<typename T>
	void Config::SetValue(
		util::CHashKey const& key, T const& value, SettingType settingType /*= SettingType::Serialized*/)
	{
		{
			std::unique_lock<std::shared_mutex> readLock(m_configValuesMutex);

			SEAssert(settingType != SettingType::Serialized ||
				key.GetKey() != nullptr ||
				m_configValues.contains(key),
				"Cannot initialize config entry with a dynamically-allocated key");

			m_configValues[key] = { value, settingType };
			if (settingType == SettingType::Serialized)
			{
				m_isDirty = true;
			}
		}
	}


	template<typename T>
	bool Config::TrySetValue(
		util::CHashKey const& key, T const&  value, SettingType settingType /*= SettingType::Serialized*/)
	{
		if (KeyExists(key))
		{
			return false;
		}
		
		SetValue(key, value, settingType);
		return true;
	}


	template<typename T>
	inline std::string Config::PropertyToConfigString(T property)
	{
		return std::format(" {}\n", property);
	}


	template<>
	inline std::string Config::PropertyToConfigString(std::string const& property)
	{
		return std::format(" \"{}\"\n", property);
	}


	template<>
	inline std::string Config::PropertyToConfigString(char const* property)
	{
		return std::format(" \"{}\"\n", property);
	}


	template<>
	inline std::string Config::PropertyToConfigString(char property)
	{
		return std::format(" \"{}\"\n", property);
	}


	template<>
	inline std::string Config::PropertyToConfigString(bool property)
	{
		return std::format(" {}\n", property ? k_trueString : k_falseString);
	}


	template<>
	inline std::string Config::PropertyToConfigString(float property)
	{
		std::string const& result = std::format(" {}\n", property);
		if (result.find('.') == std::string::npos)
		{
			return std::format(" {}.0\n", property); // Ensure we have a trailing decimal point
		}
		return result;
	}
}


