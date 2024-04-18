// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "LogManager.h"


// Static asserts are defined for all build configurations:
#define SEStaticAssert(condition, msg) \
	static_assert(condition, msg);


#if defined(_DEBUG)

// TODO: Move the Win32-specific stuff to a platform wrapper (ClipCursor, ShowCursor)
// TODO: Reverse the argument order, so we can support variadic arguments in the message: condition, msg, args

static void HandleAssertInternal();

#define SEAssert(condition, errorMsg) \
	if(!(condition)) \
	{ \
		void HandleAssertInternal(); \
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str()); \
		std::cerr << "Assertion failed: " << #condition << " == " << (condition ? "true" : "false") << std::endl; \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort(); \
	}
#define SEAssertF(errorMsg) \
		void HandleAssertInternal(); \
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str()); \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort();
#else
#define SEAssert(condition, errorMsg)	\
	do { static_cast<void>(errorMsg); static_cast<void>(condition); } while (0);
#define SEAssertF(errorMsg)	\
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str());
#endif