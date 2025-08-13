// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "Logger.h"
#include "EventManager.h"

#include "Definitions/EventKeys.h"
#include "Definitions/ForwardDeclarations.h"

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
		static void SetCommandLineArgs(int argc, char** argv);
		static void ProcessCommandLineArgs();

		static void LoadConfigFile(); // Load the config file
		static void SaveConfigFile(); // Save config file to disk


	public:
		template<typename T>
		static T GetValue(util::CHashKey const&);

		template<typename T>
		static bool TryGetValue(util::CHashKey const&, T& valueOut);

		static bool KeyExists(util::CHashKey const&);

		static std::string GetValueAsString(util::CHashKey const&);
		static std::wstring GetValueAsWString(util::CHashKey const&);
	

	public:
		template<typename T>
		static void SetValue(util::CHashKey const&, T const& value, SettingType = SettingType::Serialized);

		// Set a new config value, IFF it doesn't already exist. Returns true if the value was set
		template<typename T>
		static bool TrySetValue(util::CHashKey const&, T const& value, SettingType = SettingType::Serialized);

		static void ClearValue(util::CHashKey const&);


	private:
		static void InitializeOSValues(); // Values loaded from the OS. Not saved to disk.
		static void InitializeDefaultValues(); // Initialize any unpopulated configuration values with defaults
		static void SetRuntimeDefaults(); // Platform-agnostic defaults not loaded/saved to disk
		

	private:
		using ConfigValue = std::variant<bool, int, float, char, char const*, std::string, platform::RenderingAPI, util::CHashKey>;
		static std::unordered_map<util::CHashKey, std::pair<ConfigValue, SettingType>> s_configValues;
		static std::shared_mutex s_configValuesMutex;
		static bool s_isDirty; // Marks whether we need to save the config file or not

		// Command line arguments:
		static int s_argc;
		static char** s_argv;


	private: // Helper functions:
		template<typename T>
		static std::string PropertyToConfigString(T property);

		template<>
		static std::string PropertyToConfigString(std::string const&);

		template<>
		static std::string PropertyToConfigString(char const*);

		template<>
		static std::string PropertyToConfigString(char);

		template<>
		static std::string PropertyToConfigString(bool);

		template<>
		static std::string PropertyToConfigString(float);


	private:
		static constexpr char const* k_trueString = "true";
		static constexpr char const* k_falseString = "false";
		static constexpr char const* k_setCmd = "set"; // Set a value
		static constexpr char const* k_bindCmd = "bind"; // Bind a key


	private:
		Config() = delete;
		Config(Config&&) noexcept = delete;
		Config& operator=(Config&&) noexcept = delete;
		Config(Config const&) = delete;
		Config& operator=(Config const&) = delete;
	};


	// Get a config value, by type
	template<typename T>
	T Config::GetValue(util::CHashKey const& key)
	{
		T returnVal{};
		{
			std::shared_lock<std::shared_mutex> readLock(s_configValuesMutex);

			bool foundValue = false;
			auto const& result = s_configValues.find(key);
			if (result != s_configValues.end())
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
	bool Config::TryGetValue(util::CHashKey const& key, T& valueOut)
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
			std::unique_lock<std::shared_mutex> readLock(s_configValuesMutex);

			SEAssert(settingType != SettingType::Serialized ||
				key.GetKey() != nullptr ||
				s_configValues.contains(key),
				"Cannot initialize config entry with a dynamically-allocated key");

			SEAssert(!s_configValues.contains(key) || s_configValues.at(key).second == settingType,
				"settingType does not match the current SettingType");

			s_configValues[key] = { value, settingType };
			if (settingType == SettingType::Serialized)
			{
				s_isDirty = true;
			}
		}

		// Notify listeners that a config value has changed:
		core::EventManager::Notify(core::EventManager::EventInfo{
			.m_eventKey = eventkey::ConfigSetValue,
			.m_data = key });
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


	inline void Config::ClearValue(util::CHashKey const& key)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(s_configValuesMutex);
			
			SEAssert(s_configValues.contains(key), "Trying to clear a key that is not found");

			// Only mark as dirty if we're deleting a serialized value
			s_isDirty = s_configValues.at(key).second == SettingType::Serialized;

			s_configValues.erase(key);
		}
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


