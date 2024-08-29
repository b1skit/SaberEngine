// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	// Note: fileExtension includes the period (e.g. ".exampleExt"), or nullptr for all files regardless of extension
	std::vector<std::string> GetDirectoryFilenameContents(
		char const* directoryPath, char const* fileExtension = nullptr);


	enum class BuildConfiguration
	{
		Debug,
		DebugRelease,
		Profile,
		Release,

		Invalid,
	};
	BuildConfiguration CStrToBuildConfiguration(char const* buildConfigCStr);

	BuildConfiguration GetBuildConfigurationMarker(std::string const& path);
	void SetBuildConfigurationMarker(std::string const& path, BuildConfiguration);
}