// © 2025 Adam Badke. All rights reserved.
#include "Dialog.h"
#include "Dialog_Platform.h"


namespace host
{
	bool Dialog::OpenFileDialogBox(
		std::string const& filterName,
		std::vector<std::string> const& allowedExtensions,
		std::string& filepathOut)
	{
		return platform::Dialog::OpenFileDialogBox(
			filterName,
			allowedExtensions,
			filepathOut);
	}
}