// © 2022 Adam Badke. All rights reserved.
#pragma once


// Add this to the head of a file (after includes) to disable optimizations for the whole file. This is useful for
// debugging optimized builds
#define SE_DISABLE_OPTIMIZATIONS \
	_Pragma("optimize(\"\", off)");


// Enable this to print failed asserts as messages when _DEBUG is not enabled. Warning: Do not leave this enabled
//#define RELEASE_ASSERTS_AS_LOG_ERRORS


// Static asserts are defined for all build configurations:
#define SEStaticAssert(condition, msg) \
	static_assert(condition, msg);


void HandleLogError(char const*); // Wrapper for the LOG_ERROR macro, as we can't include LogManager.h here


#if defined(_DEBUG)

// TODO: Move the Win32-specific stuff to a platform wrapper (ClipCursor, ShowCursor)
// TODO: Support variadic arguments in the message

void HandleAssertInternal();


#define SEAssert(condition, errorMsg) \
	if(!(condition)) \
	{ \
		void HandleAssertInternal(); \
		const std::string errorStr((errorMsg)); \
		HandleLogError(errorStr.c_str()); \
		std::cerr << "Assertion failed: " << #condition << " == " << (condition ? "true" : "false") << std::endl; \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort(); \
	}

#define SEAssertF(errorMsg) \
	{ \
		void HandleAssertInternal(); \
		const std::string errorStr((errorMsg)); \
		HandleLogError(errorStr.c_str()); \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort(); \
	}

#define SEFatalAssert(condition, errorMsg) SEAssert(condition, errorMsg)

#else

#if defined(RELEASE_ASSERTS_AS_LOG_ERRORS)

	#define SEAssert(condition, errorMsg)	\
		if (!(condition)) HandleLogError(std::string(errorMsg).c_str());

#else

#define SEAssert(condition, errorMsg)	\
	do { static_cast<void>(condition); } while (0);


#define SEFatalAssert(condition, errorMsg)	\
	if(!(condition)) \
	{ \
		void HandleAssertInternal(); \
		const std::string errorStr((errorMsg)); \
		HandleLogError(errorStr.c_str()); \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort(); \
	}

#endif // RELEASE_ASSERTS_AS_LOG_ERRORS

#define SEAssertF(errorMsg)	\
	{ \
		HandleLogError(std::string(errorMsg).c_str()); \
	}

#endif // _DEBUG