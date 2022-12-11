#pragma once

#include <string>
#include <fstream>


namespace util
{
	std::string LoadTextAsString(std::string const& filepath)
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
}