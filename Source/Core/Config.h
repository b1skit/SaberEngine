// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "LogManager.h"

#include "Definitions/ConfigKeys.h"

#include "Util/HashKey.h"


namespace core
{
	class Config
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

		void ProcessCommandLineArgs(int argc, char** argv);

		void LoadConfigFile(); // Load the config file
		void SaveConfigFile(); // Save config file to disk


	public:
		template<typename T>
		T GetValue(util::HashKey const&) const;

		template<typename T>
		bool TryGetValue(util::HashKey const&, T& valueOut) const;

		bool KeyExists(util::HashKey const&) const;

		std::string GetValueAsString(util::HashKey const&) const;
		std::wstring GetValueAsWString(util::HashKey const&) const;
	

	public:
		template<typename T>
		void SetValue(util::HashKey const&, T const& value, SettingType = SettingType::Serialized);

		// Set a new config value, IFF it doesn't already exist. Returns true if the value was set
		template<typename T>
		bool TrySetValue(util::HashKey const&, T const& value, SettingType = SettingType::Serialized);


	private:
		void InitializeOSValues(); // Values loaded from the OS. Not saved to disk.
		void InitializeDefaultValues(); // Initialize any unpopulated configuration values with defaults
		void SetRuntimeDefaults(); // Platform-agnostic defaults not loaded/saved to disk
		

	private:
		std::unordered_map<util::HashKey const, std::pair<std::any, SettingType>> m_configValues;
		mutable std::shared_mutex m_configValuesMutex;
		bool m_isDirty; // Marks whether we need to save the config file or not


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


	private: // No copying allowed
		Config(Config const&) = delete;
		Config& operator=(Config const&) = delete;
		Config(Config&&) = delete;
		Config& operator=(Config&&) = delete;
	};


	// Get a config value, by type
	template<typename T>
	T Config::GetValue(util::HashKey const& key) const
	{
		T returnVal{};
		{
			std::shared_lock<std::shared_mutex> readLock(m_configValuesMutex);

			auto const& result = m_configValues.find(key);
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
				LOG_ERROR("Config::GetValue: Key does not exist");
			}
		}
		return returnVal;
	}


	template<typename T>
	bool Config::TryGetValue(util::HashKey const& key, T& valueOut) const
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
		util::HashKey const& key, T const& value, SettingType settingType /*= SettingType::Serialized*/)
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
		util::HashKey const& key, T const&  value, SettingType settingType /*= SettingType::Serialized*/)
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


