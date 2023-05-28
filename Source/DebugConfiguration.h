// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "LogManager.h"


// Optional checks in debug mode:
#if defined(_DEBUG)
	//#define DEBUG_LOG_OPENGL_NOTIFICATIONS // Enable non-essential OpenGL notification logging
#endif


// Custom assert:
#if defined(_DEBUG)

// TODO: Move the Win32-specific stuff to a platform wrapper (ClipCursor, ShowCursor)
// TODO: Reverse the argument order, so we can support variadic arguments in the message: condition, msg, args

#define SEAssert(errorMsg, condition) \
	if(!(condition)) \
	{ \
		ClipCursor(NULL); ShowCursor(true); /* Disable relative mouse mode* */ \
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str()); \
		std::cerr << "Assertion failed: " << #condition << " == " << (condition ? "true" : "false") << std::endl; \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort(); \
	}
#define SEAssertF(errorMsg) \
		ClipCursor(NULL); ShowCursor(true); /* Disable relative mouse mode* */ \
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str()); \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort();
#else
#define SEAssert(errorMsg, condition)	\
	do {static_cast<void>(errorMsg); const bool supressCompilerWarningByUsingCondition = condition;} while(0)
#define SEAssertF(errorMsg)	\
	do {static_cast<void>(errorMsg);} while(0)
#endif


