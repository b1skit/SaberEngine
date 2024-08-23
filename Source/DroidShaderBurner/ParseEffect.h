// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace droid
{
	enum ErrorCode : int
	{
		Success = 0,

		FileError = -1,
		JSONError = -2,
		DataError = -3,
	};
	extern constexpr char const* ErrorCodeToCStr(ErrorCode);

	struct ParseParams;
	ErrorCode DoParsingAndCodeGen(ParseParams const&);
}