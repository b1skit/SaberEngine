// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace droid
{
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
	};
	extern constexpr char const* ErrorCodeToCStr(ErrorCode);


	struct ParseParams;
	ErrorCode DoParsingAndCodeGen(ParseParams const&);

	time_t GetMostRecentlyModifiedFileTime(std::string const& filesystemTarget);

	void CleanDirectory(std::string const& dirPath, bool recreateDir = true);
}