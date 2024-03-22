// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	// Note: fileExtension includes the period (e.g. ".exampleExt"), or nullptr for all files regardless of extension
	std::vector<std::string> GetDirectoryFilenameContents(char const* directoryPath, char const* fileExtension = nullptr)
	{
		std::vector<std::string> directoryFileContents;

		for (const auto& directoryEntry : std::filesystem::directory_iterator(directoryPath))
		{
			std::string const& directoryEntryStr = directoryEntry.path().string();

			if (!fileExtension ||
				strcmp(std::filesystem::path(directoryEntry.path()).extension().string().c_str(), fileExtension) == 0)
			{
				directoryFileContents.emplace_back(directoryEntry.path().string());
			}
		}

		return directoryFileContents;
	}
}