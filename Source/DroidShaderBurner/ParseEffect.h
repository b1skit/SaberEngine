// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace droid
{
	enum ErrorCode : int
	{
		Success = 0,

		FileError = -1,				// E.g. Can't find/open a file
		JSONError = -2,				// E.g. JSON contains a structural error
		DataError = -3,				// E.g. Bitmask overflow: Generated data is bad
		ConfigurationError = -4,	// E.g. Bad command line arg
	};
	extern constexpr char const* ErrorCodeToCStr(ErrorCode);

	struct ParseParams;
	ErrorCode DoParsingAndCodeGen(ParseParams const&);
}