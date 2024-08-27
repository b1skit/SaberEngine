// © 2024 Adam Badke. All rights reserved.
#include "ParseDB.h"
#include "EffectParsing.h"

#include "Core/Definitions/EffectKeys.h"


namespace droid
{
	constexpr char const* ErrorCodeToCStr(ErrorCode errorCode)
	{
		switch (errorCode)
		{
		case ErrorCode::Success: return "Success";
		case ErrorCode::NoModification: return "NoModification";
		case ErrorCode::FileError: return "FileError";
		case ErrorCode::JSONError: return "JSONError";
		case ErrorCode::DataError: return "DataError";
		case ErrorCode::ConfigurationError: return "ConfigurationError";
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
		if (result != droid::ErrorCode::Success)
		{
			return result;
		}

		result = parseDB.GenerateShaderCode();
		if (result != droid::ErrorCode::Success)
		{
			return result;
		}

		return result;
	}


	void CleanDirectory(std::string const& dirPath)
	{
		std::filesystem::remove_all(dirPath);
	}
}