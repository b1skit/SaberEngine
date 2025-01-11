// © 2023 Adam Badke. All rights reserved.
#include "FileIOUtils.h"
#include "TextUtils.h"


namespace
{
	constexpr char const* k_buildConfigMarkerNames[] = {
	".debug",
	".debugrelease",
	".profile",
	".release",
	};
}

namespace util
{
	std::vector<std::string> GetDirectoryFilenameContents(
		char const* directoryPath, char const* fileExtension /*= nullptr*/)
	{
		std::vector<std::string> directoryFileContents;

		if (std::filesystem::exists(directoryPath))
		{
			for (const auto& directoryEntry : std::filesystem::directory_iterator(directoryPath))
			{
				std::string const& directoryEntryStr = directoryEntry.path().string();

				if (!fileExtension ||
					strcmp(std::filesystem::path(directoryEntry.path()).extension().string().c_str(), fileExtension) == 0)
				{
					directoryFileContents.emplace_back(directoryEntry.path().string());
				}
			}
		}

		return directoryFileContents;
	}


	bool FileExists(char const* path)
	{
		return std::filesystem::exists(path);
	}


	bool FileExists(std::string const& path)
	{
		return FileExists(path.c_str());
	}


	std::string ExtractDirectoryPathFromFilePath(std::string const& filepath)
	{
		const size_t lastSlash = filepath.find_last_of("/\\");
		return filepath.substr(0, lastSlash) + "\\";
	}


	std::string ExtractFileNameAndExtensionFromFilePath(std::string const& filepath)
	{
		const size_t lastSlash = filepath.find_last_of("/\\");
		return filepath.substr(lastSlash + 1);
	}


	BuildConfiguration CStrToBuildConfiguration(char const* buildConfigCStr)
	{
		std::string const& buildConfigCStrLower = util::ToLower(buildConfigCStr);

		if (buildConfigCStrLower == "debug") return BuildConfiguration::Debug;
		if (buildConfigCStrLower == "debugrelease") return BuildConfiguration::DebugRelease;
		if (buildConfigCStrLower == "profile") return BuildConfiguration::Profile;
		if (buildConfigCStrLower == "release") return BuildConfiguration::Release;

		return BuildConfiguration::Invalid;
	}


	BuildConfiguration GetBuildConfigurationMarker(std::string const& pathStr)
	{
		if (std::filesystem::exists(pathStr))
		{
			for (uint8_t i = 0; i < _countof(k_buildConfigMarkerNames); ++i)
			{
				std::filesystem::path markerPath = pathStr;
				markerPath /= k_buildConfigMarkerNames[i];
				if (std::filesystem::exists(markerPath))
				{
					return static_cast<BuildConfiguration>(i);
				}
			}
		}

		return BuildConfiguration::Invalid;
	}


	void SetBuildConfigurationMarker(std::string const& path, BuildConfiguration buildConfig)
	{
		std::filesystem::path markerPath = path;
		markerPath /= k_buildConfigMarkerNames[static_cast<uint8_t>(buildConfig)];
		std::ofstream markerStream;
		markerStream.open(markerPath);
		if (markerStream.is_open())
		{
			markerStream << "This file indicates the other files in this directory are suitable for use with the "
				<< k_buildConfigMarkerNames[static_cast<uint8_t>(buildConfig)] << " configuration";
			markerStream.close();
		}
	}
}