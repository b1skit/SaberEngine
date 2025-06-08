#pragma once


namespace platform
{
	class Dialog final
	{
	public:
		static bool (*OpenFileDialogBox)(
			std::string const& filterName,
			std::vector<std::string> const& allowedExtensions,
			std::string& filepathOut);
	};
}