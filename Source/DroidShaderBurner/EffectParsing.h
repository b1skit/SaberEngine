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

	// Internal ErrorCode enum for backward compatibility with existing implementation
	// Public interfaces use bool return values and exceptions
	enum class ErrorCode
	{
		Success = 0,
		NoModification = 1,
		FileError = -1,
		JSONError = -2,
		ShaderError = -3,
		GenerationError = -4,
		ConfigurationError = -5,
		DependencyError = -6,
		ComError = -7
	};

	struct ParseParams;
	bool DoParsingAndCodeGen(ParseParams const&);
}