// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace host
{
	class Dialog final
	{
	public:
		// Create an OS file open dialog. Returns true and populates filepathOut if the user selected a file
		static bool OpenFileDialogBox(
			std::string const& filterName, // E.g. "GLTF Files"
			std::vector<std::string> const& allowedExtensions, // E.g. {"*.gltf", "*.glb"}
			std::string& filepathOut);
	};
}