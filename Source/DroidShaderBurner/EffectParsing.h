// � 2024 Adam Badke. All rights reserved.
#pragma once


namespace droid
{
	// Exception classes for different error types
	class FileException : public std::runtime_error
	{
	public:
		explicit FileException(const std::string& message) : std::runtime_error(message) {}
	};

	class JSONException : public std::runtime_error
	{
	public:
		explicit JSONException(const std::string& message) : std::runtime_error(message) {}
	};

	class ShaderException : public std::runtime_error
	{
	public:
		explicit ShaderException(const std::string& message) : std::runtime_error(message) {}
	};

	class GenerationException : public std::runtime_error
	{
	public:
		explicit GenerationException(const std::string& message) : std::runtime_error(message) {}
	};

	class ConfigurationException : public std::runtime_error
	{
	public:
		explicit ConfigurationException(const std::string& message) : std::runtime_error(message) {}
	};

	class DependencyException : public std::runtime_error
	{
	public:
		explicit DependencyException(const std::string& message) : std::runtime_error(message) {}
	};

	class ComException : public std::runtime_error
	{
	public:
		explicit ComException(const std::string& message) : std::runtime_error(message) {}
	};

	struct ParseParams;
	bool DoParsingAndCodeGen(ParseParams const&);
}