// � 2024 Adam Badke. All rights reserved.
#pragma once

#include <exception>
#include <string>


namespace droid
{
	// Exception hierarchy for DroidShaderBurner
	class DroidException : public std::exception
	{
	public:
		explicit DroidException(std::string const& message) : m_message(message) {}
		char const* what() const noexcept override { return m_message.c_str(); }
	private:
		std::string m_message;
	};

	class FileException : public DroidException
	{
	public:
		explicit FileException(std::string const& message) : DroidException("File error: " + message) {}
	};

	class JSONException : public DroidException
	{
	public:
		explicit JSONException(std::string const& message) : DroidException("JSON error: " + message) {}
	};

	class ShaderException : public DroidException
	{
	public:
		explicit ShaderException(std::string const& message) : DroidException("Shader error: " + message) {}
	};

	class GenerationException : public DroidException
	{
	public:
		explicit GenerationException(std::string const& message) : DroidException("Generation error: " + message) {}
	};

	class ConfigurationException : public DroidException
	{
	public:
		explicit ConfigurationException(std::string const& message) : DroidException("Configuration error: " + message) {}
	};

	class DependencyException : public DroidException
	{
	public:
		explicit DependencyException(std::string const& message) : DroidException("Dependency error: " + message) {}
	};

	class ComException : public DroidException
	{
	public:
		explicit ComException(std::string const& message) : DroidException("COM error: " + message) {}
	};

	// Special exception for "no modification needed" cases
	class NoModificationResult : public DroidException
	{
	public:
		explicit NoModificationResult(std::string const& message) : DroidException(message) {}
	};

	enum ErrorCode : int
	{
		Success = 0,
		NoModification = 1,

		FileError			= -1,	// E.g. Can't find/open a file
		JSONError			= -2,	// E.g. JSON contains a structural error
		ShaderError			= -3,	// E.g. HLSL compiler returned an error code
		GenerationError		= -4,	// E.g. Bitmask overflow: Generated data is bad
		ConfigurationError	= -5,	// E.g. Bad command line arg
		DependencyError		= -6,	// E.g. Error invoking an external process, or the process returned an error
		ComError			= -7,	// E.g. COM interface error when using DXC API
	};
	extern constexpr char const* ErrorCodeToCStr(ErrorCode);


	struct ParseParams;
	void DoParsingAndCodeGen(ParseParams const&);
}