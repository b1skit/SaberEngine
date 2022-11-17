#pragma once

#include <string>
#include <unordered_map>
#include <any>
#include <memory>
#include "Platform.h"


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


	public: // Singleton functionality:		
		static Config* Get();
	private:
		static std::unique_ptr<Config> m_instance;


	public:
		Config();		

		// Get a config value, by type
		template<typename T>
		T GetValue(const std::string& valueName) const;
		std::string GetValueAsString(const std::string& valueName) const;

		// Set a config value. Note: Strings must be explicitely defined as a string("value")
		template<typename T>
		void SetValue(const std::string& valueName, T value, SettingType settingType = SettingType::Common);

		// Load the config.cfg file
		void LoadConfig();

		// Save config.cfg to disk
		void SaveConfig();


		// Specific configuration retrieval:
		/**********************************/

		// Compute the aspect ratio == width / height
		inline float GetWindowAspectRatio() const
		{
			return (float)(GetValue<int>("windowXRes")) / (float)(GetValue<int>("windowYRes"));
		}

		inline const platform::RenderingAPI& GetRenderingAPI() const { return m_renderingAPI; }


	private:
		// Initialize the configValues mapping with default values. MUST be called before the config can be accessed.
		// Note: Default values should be set inside here.
		void InitializeDefaultValues();

		void SetAPIDefaults();

		std::unordered_map<std::string, std::pair<std::any, SettingType>> m_configValues;	// The config parameter/value map
		bool m_isDirty; // Marks whether we need to save the config file or not


		// Explicit members (for efficiency):
		/***********************************/
		platform::RenderingAPI m_renderingAPI;

		const std::string m_configDir = "..\\config\\";
		const std::string m_configFilename = "config.cfg";


		// Helper functions:
		//------------------
		inline std::string PropertyToConfigString(std::string property)	{ return " \"" + property + "\"\n"; }
		inline std::string PropertyToConfigString(float property) { return " " + std::to_string(property) + "\n"; }
		inline std::string PropertyToConfigString(int property) { return " " + std::to_string(property) + "\n"; }
		inline std::string PropertyToConfigString(char property) {return std::string(" ") + property + std::string("\n");}
		
		// Note: Inlined in .cpp file, as it depends on macros defined in KeyConfiguration.h
		std::string PropertyToConfigString(bool property);
	};
}


