// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	inline std::string LoadTextAsString(std::string const& filepath)
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

	inline std::wstring ToWideString(std::string const& str)
	{
		return std::wstring(str.begin(), str.end());
	}


	inline std::string FromWideString(std::wstring const& wstr)
	{
		// Note: This is function is deprecated (we squash the warning in the pch) TODO: Handle this correctly
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wstringConverter;
		return wstringConverter.to_bytes(wstr);
	}


	inline std::string GetTimeAndDateAsString()
	{
		auto time = std::time(nullptr);
		auto tm = *std::localtime(&time);

		std::stringstream result;
		result << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
		return result.str();
	}
}