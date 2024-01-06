// � 2022 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	static std::string LoadTextAsString(std::string const& filepath)
	{
		std::ifstream file;
		file.open(filepath.c_str());

		std::string output;
		std::string line;
		if (file.is_open())
		{
			while (file.good())
			{
				getline(file, line);
				output.append(line + "\n");
			}
		}
		else
		{
			return "";
		}

		return output;
	}


	static std::wstring ToWideString(std::string const& str)
	{
		return std::wstring(str.begin(), str.end());
	}


	static std::string FromWideCString(wchar_t const* wstr, size_t wstrLen)
	{
		std::string result;
		result.resize(wstrLen);
		std::wcstombs(result.data(), wstr, wstrLen);

		return result;
	}


	static std::string FromWideCString(wchar_t const* wstr)
	{
		return FromWideCString(wstr, std::wcslen(wstr));
	}


	static std::string FromWideString(std::wstring const& wstr)
	{
		return FromWideCString(wstr.c_str());
	}


	static std::string GetTimeAndDateAsString()
	{
		auto time = std::time(nullptr);
		auto tm = *std::localtime(&time);

		std::stringstream result;
		result << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
		return result.str();
	}
}