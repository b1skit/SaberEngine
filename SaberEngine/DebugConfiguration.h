#pragma once

#include "LogManager.h"


// Optional checks in debug mode:
#if defined(_DEBUG)
	// Assert if textures/PBs aren't found when attempting to bind them. Helpful, but can be annoying
	#define STRICT_SHADER_BINDING
	
#endif

// Custom assert:
#if defined(_DEBUG)

#define SEAssert(errorMsg, condition) \
	if(!(condition)) \
	{ \
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str()); \
		std::cerr << "Assertion failed: " << #condition << " == " << (condition ? "true" : "false") << std::endl; \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		std::abort(); \
	}

#define SEAssertF(errorMsg) \
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




#if defined(_DEBUG)
	// OpenGL-specific logging (in RenderManager.cpp)
	//-----------------------------------------------
	#define DEBUG_LOG_OPENGL						// Enable/disable OpenGL logging
	#if defined(DEBUG_LOG_OPENGL)
		//#define DEBUG_LOG_OPENGL_NOTIFICATIONS	// Enable non-essential OpenGL notification logging
	#endif

#endif