#pragma once

#include <string>
#include <unordered_map>
#include <any>

using std::to_string;
using std::unordered_map;
using std::any;

namespace
{
	// Convert a string to lower case: Used to simplify comparisons
	inline std::string ToLowerCase(std::string input)
	{
		std::string output;
		for (auto currentChar : input)
		{
			output += std::tolower(currentChar);
		}
		return output;
	}
}


namespace SaberEngine
{
	struct EngineConfig
	{
		EngineConfig();

		// Initialize the configValues mapping with default values. MUST be called before the config can be accessed.
		// Set all default values here.
		void InitializeDefaultValues();

		// Get a config value, by type
		template<typename T>
		T GetValue(const std::string& valueName) const;
		std::string GetValueAsString(const std::string& valueName) const;

		// Set a config value. Note: Strings must be explicitely defined as a string("value")
		template<typename T>
		void SetValue(const std::string& valueName, T value); 

		// Compute the aspect ratio == width / height
		float GetWindowAspectRatio() const 
			{ return (float)(GetValue<int>("windowXRes")) / (float)(GetValue<int>("windowYRes")); }

		// Load the config.cfg from CONFIG_FILENAME
		void LoadConfig();

		// Save config.cfg to disk
		void SaveConfig();

		// Currently loaded scene (cached during command-line parsing, accessed once SceneManager is loaded)
		std::string m_currentScene = ""; 

	private:
		unordered_map<std::string, any> m_configValues;	// The primary config parameter/value mapping


		const std::string CONFIG_DIR		= ".\\config\\";
		const std::string CONFIG_FILENAME	= "config.cfg";
		
		bool m_isDirty = false; // Marks whether we need to save the config file or not


		// Inline helper functions:
		//------------------
		inline std::string PropertyToConfigString(std::string property)	{ return " \"" + property + "\"\n"; }
		inline std::string PropertyToConfigString(float property)	{ return " " + std::to_string(property) + "\n"; }
		inline std::string PropertyToConfigString(int property)		{ return " " + std::to_string(property) + "\n"; }
		inline std::string PropertyToConfigString(char property) {return std::string(" ") + property + std::string("\n");}

		// Note: Inlined in .cpp file, as it depends on macros defined in KeyConfiguration.h
		std::string PropertyToConfigString(bool property);	
	};
}


