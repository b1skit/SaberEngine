// � 2024 Adam Badke. All rights reserved.
#pragma once


// © 2024 Adam Badke. All rights reserved.
#pragma once
#include <stdexcept>
#include <string>


namespace droid
{
	// Base exception class for all DroidShaderBurner errors
	class DroidException : public std::exception
	{
	public:
		explicit DroidException(std::string const& message) : m_message(message) {}
		char const* what() const noexcept override { return m_message.c_str(); }
		
	private:
		std::string m_message;
	};

	// Specific exception types for different error categories
	class FileException : public DroidException
	{
	public:
		explicit FileException(std::string const& message) : DroidException("File Error: " + message) {}
	};

	class JSONException : public DroidException
	{
	public:
		explicit JSONException(std::string const& message) : DroidException("JSON Error: " + message) {}
	};

	class ShaderException : public DroidException
	{
	public:
		explicit ShaderException(std::string const& message) : DroidException("Shader Error: " + message) {}
	};

	class GenerationException : public DroidException
	{
	public:
		explicit GenerationException(std::string const& message) : DroidException("Generation Error: " + message) {}
	};

	class ConfigurationException : public DroidException
	{
	public:
		explicit ConfigurationException(std::string const& message) : DroidException("Configuration Error: " + message) {}
	};

	class DependencyException : public DroidException
	{
	public:
		explicit DependencyException(std::string const& message) : DroidException("Dependency Error: " + message) {}
	};

	class ComException : public DroidException
	{
	public:
		explicit ComException(std::string const& message) : DroidException("COM Error: " + message) {}
	};

	// Special result type for successful completion without modification
	class NoModificationResult : public std::exception
	{
	public:
		char const* what() const noexcept override { return "No modification required"; }
	};


	struct ParseParams;
	void DoParsingAndCodeGen(ParseParams const&);
}