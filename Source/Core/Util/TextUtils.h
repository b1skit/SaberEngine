// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	std::string LoadTextAsString(std::string const& filepath);

	std::wstring ToWideString(std::string const& str);

	std::string FromWideCString(std::span<const wchar_t> wstr);
	std::string FromWideCString(wchar_t const* wstr, size_t wstrLen);
	std::string FromWideCString(wchar_t const* wstr);

	std::string FromWideString(std::wstring const& wstr);

	std::string GetTimeAndDateAsString();

	std::string ToLower(std::string_view str);
}