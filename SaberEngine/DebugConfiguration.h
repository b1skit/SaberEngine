#pragma once

#include "LogManager.h"


// Optional checks in debug mode:
#if defined(_DEBUG)
	// Assert if textures/PBs aren't found when attempting to bind them. Helpful, but can be annoying
	#define STRICT_SHADER_BINDING
	
#endif

// Custom assert:
#if defined(_DEBUG)

#include <iostream>
#include <string>

#define SEAssert(errorMsg, condition) \
	if(!(condition)) \
	{ \
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str()); \
		std::cerr << "Assertion failed: " << #condition << " == " << (condition ? "true" : "false") << std::endl; \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		abort(); \
	}

#define SEAssertF(errorMsg) \
		const std::string errorStr((errorMsg)); \
		LOG_ERROR(errorStr.c_str()); \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		abort();
#else
#define SEAssert(errorMsg, condition)	\
	do {errorMsg; const bool supressCompilerWarningByUsingCondition = condition;} while(0)
#endif


#if defined(_DEBUG)
	// Event logging:
	//---------------
	//#define DEBUG_PRINT_NOTIFICATIONS				// Print notifications as the event manager receives them
	
	//#define DEBUG_LOGMANAGER_LOG_EVENTS				// Print events as the log manager receives them. Logged events are configured below
	#if defined (DEBUG_LOGMANAGER_LOG_EVENTS)
		#define DEBUG_LOGMANAGER_KEY_INPUT_LOGGING		// Log keypress input events
		#define DEBUG_LOGMANAGER_MOUSE_INPUT_LOGGING	// Log mouse input events
		#define DEBUG_LOGMANAGER_QUIT_LOGGING			// Log quit 
	#endif
	
	// OpenGL-specific logging (in RenderManager.cpp)
	//-----------------------------------------------
	#define DEBUG_LOG_OPENGL						// Enable/disable OpenGL logging
	#if defined(DEBUG_LOG_OPENGL)
		//#define DEBUG_LOG_OPENGL_NOTIFICATIONS	// Enable non-essential OpenGL notification logging
	#endif

#endif