// © 2025 Adam Badke. All rights reserved.
#include "Private/Dialog_Platform.h"


namespace platform
{
	bool (*Dialog::OpenFileDialogBox)(
		std::string const& filterName,
		std::vector<std::string> const& allowedExtensions,
		std::string& filepathOut) = nullptr;
}