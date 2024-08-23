// © 2024 Adam Badke. All rights reserved.
#include "ParseDB.h"
#include "ParseEffect.h"

#include "Core/Definitions/EffectKeys.h"


namespace
{
	droid::ErrorCode ParseEffect(std::string const& effectFilePath, droid::ParseDB& parseDB)
	{
		std::ifstream effectStream(effectFilePath);
		if (!effectStream.is_open())
		{
			std::cout << "Error: Failed to open Effect file \"" << effectFilePath.c_str() << "\"\n";
			return droid::ErrorCode::FileError;
		}
		std::cout << "Successfully opened file \"" << effectFilePath.c_str() << "\"!\n\n";



		effectStream.close();
		return droid::ErrorCode::Success;
	}
}

namespace droid
{
	constexpr char const* ErrorCodeToCStr(ErrorCode errorCode)
	{
		switch (errorCode)
		{
		case ErrorCode::Success: return "Success";
		case ErrorCode::FileError: return "FileError";
		case ErrorCode::JSONError: return "JSONError";
		default: return "INVALID_ERROR_CODE";
		}
	}


	ErrorCode DoParsingAndCodeGen(ParseParams const& parseParams)
	{
		ParseDB parseDB(parseParams);

		droid::ErrorCode result = parseDB.Parse();
		if (result != droid::ErrorCode::Success)
		{
			return result;
		}

		result = parseDB.GenerateCPPCode();

		return result;
	}
}