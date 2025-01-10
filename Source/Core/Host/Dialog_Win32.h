// © 2025 Adam Badke. All rights reserved.
#pragma once

//struct COMDLG_FILTERSPEC;

namespace win32
{
	class Dialog
	{
	public:
		static bool OpenFileDialogBox(
			std::string const& filterName,
			std::vector<std::string> const& allowedExtensions,
			std::string& filepathOut);
	};
}