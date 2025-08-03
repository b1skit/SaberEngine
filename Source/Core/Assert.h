// © 2022 Adam Badke. All rights reserved.
#pragma once


/**********************************************************************************************************************
*	Debugging helpers:
***********************************************************************************************************************/


// Add this to the head of a file (after includes) to disable optimizations for the whole file. This is useful for
// debugging optimized builds
#define SE_DISABLE_OPTIMIZATIONS \
	_Pragma("optimize(\"\", off)");


/**********************************************************************************************************************
*	Assert macros:
***********************************************************************************************************************/


namespace assertinternal
{
	void HandleAssertInternal(char const*);

	void LogAssertAsError(char const*);

	std::string StringFromVariadicArgs(char const* msg, ...);
}


// Enable this to print failed asserts as messages when _DEBUG is not enabled. Warning: Do not leave this enabled
//#define RELEASE_ASSERTS_AS_LOG_ERRORS


// Static asserts are defined for all build configurations:
#define SEStaticAssert(condition, msg) \
	static_assert(condition, msg);


// SEVerify will always evaluate the condition, but prints an error message instead of aborting in non-_DEBUG builds
#if defined(_DEBUG)

#define SEVerify(condition, errorMsg, ...) \
	if(!(condition)) \
	{ \
		std::string const& errorStr = std::format("\n\n\n\n\nAssertion failed: {} == {}\n\"{}\"\nFile: {}\nLine: {}\nFunction: {}\n\n\n", \
			#condition, \
			(condition ? "true" : "false"), \
			assertinternal::StringFromVariadicArgs(errorMsg, __VA_ARGS__), \
			__FILE__, \
			__LINE__, \
			__func__ \
		); \
		assertinternal::HandleAssertInternal(errorStr.c_str()); \
		std::cerr << errorStr.c_str(); \
		std::abort(); \
	}

#else

#define SEVerify(condition, errorMsg, ...) \
	if(!(condition)) \
	{ \
		std::string const& errorStr = std::format("\n\n\n\n\nVerification failed: {} == {}\n\"{}\"\nFile: {}\nLine: {}\nFunction: {}\n\n\n", \
			#condition, \
			(condition ? "true" : "false"), \
			assertinternal::StringFromVariadicArgs(errorMsg, __VA_ARGS__), \
			__FILE__, \
			__LINE__, \
			__func__ \
		); \
		assertinternal::LogAssertAsError(errorStr.c_str()); \
	}

#endif


#if defined(_DEBUG)

#define SEAssert(condition, errorMsg, ...) \
	if(!(condition)) \
	{ \
		std::string const& errorStr = std::format( \
			"\n\n================ ASSERTION FAILED =================\n" \
			"Condition: {} == {}\n" \
			"Message: \"{}\"\n" \
			"File: {}\n" \
			"Line: {}\n" \
			"Function: {}\n" \
			"===================================================\n\n", \
			#condition, \
			(condition ? "true" : "false"), \
			assertinternal::StringFromVariadicArgs(errorMsg, __VA_ARGS__), \
			__FILE__, \
			__LINE__, \
			__func__ \
		); \
		assertinternal::HandleAssertInternal(errorStr.c_str()); \
		std::cerr << errorStr.c_str(); \
		std::abort(); \
	}

#elif defined(RELEASE_ASSERTS_AS_LOG_ERRORS)

#define SEAssert(condition, errorMsg, ...)	\
		if (!(condition)) \
		{ \
		std::string const& errorStr = std::format( \
			"\n\n================ ASSERTION FAILED =================\n" \
			"Condition: {} == {}\n" \
			"Message: \"{}\"\n" \
			"File: {}\n" \
			"Line: {}\n" \
			"Function: {}\n" \
			"===================================================\n\n", \
				#condition, \
				(condition ? "true" : "false"), \
				assertinternal::StringFromVariadicArgs(errorMsg, __VA_ARGS__), \
				__FILE__, \
				__LINE__, \
				__func__ \
			); \
			assertinternal::LogAssertAsError(errorStr.c_str()); \
			std::cerr << errorStr.c_str(); \
		}

#else

#define SEAssert(condition, errorMsg, ...)

#endif


#if defined(_DEBUG)

#define SEAssertF(errorMsg, ...) \
	{ \
		std::string const& errorStr = std::format( \
			"\n\n==================== ASSERTION ====================\n" \
			"Message: \"{}\"\n" \
			"File: {}\n" \
			"Line: {}\n" \
			"Function: {}\n" \
			"===================================================\n\n", \
			assertinternal::StringFromVariadicArgs(errorMsg, __VA_ARGS__), \
			__FILE__, \
			__LINE__, \
			__func__ \
		); \
		assertinternal::HandleAssertInternal(errorStr.c_str()); \
		std::cerr << errorStr.c_str(); \
		std::abort(); \
	}
#else

#define SEAssertF(errorMsg, ...) \
		std::string const& errorStr = std::format( \
			"\n\n==================== ASSERTION ====================\n" \
			"Message: \"{}\"\n" \
			"File: {}\n" \
			"Line: {}\n" \
			"Function: {}\n" \
			"===================================================\n\n", \
			assertinternal::StringFromVariadicArgs(errorMsg, __VA_ARGS__), \
			__FILE__, \
			__LINE__, \
			__func__ \
		); \
		assertinternal::LogAssertAsError(errorStr.c_str());

#endif


// SEFatalAssert is always active in all build configurations
#define SEFatalAssert(condition, errorMsg, ...)	\
	if(!(condition)) \
	{ \
		std::string const& errorStr = std::format( \
			"\n\n================== FATAL ASSERT ===================\n" \
			"Condition: {} == {}\n" \
			"Message: \"{}\"\n" \
			"File: {}\n" \
			"Line: {}\n" \
			"Function: {}\n" \
			"===================================================\n\n", \
			#condition, \
			(condition ? "true" : "false"), \
			assertinternal::StringFromVariadicArgs(errorMsg, __VA_ARGS__), \
			__FILE__, \
			__LINE__, \
			__func__ \
		); \
		assertinternal::HandleAssertInternal(errorStr.c_str()); \
		std::cerr << errorStr.c_str(); \
		std::abort(); \
	}