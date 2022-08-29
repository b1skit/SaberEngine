#pragma once

#include "LogManager.h"


// Custom assert:
#if defined(_DEBUG)

#include <iostream>
#include <string>

#define SEAssert(errorMsg, condition) \
	if(!(condition)) \
	{ \
		LOG_ERROR(errorMsg); \
		std::string errorStr((errorMsg)); \
		std::cerr << "Assertion failed: " << #condition << " == " << (condition ? "true" : "false") << std::endl; \
		std::cerr << "Occurred at: " << __FILE__ << ":" << __LINE__ << "::" << __FUNCTION__ << std::endl; \
		abort(); \
	}
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
		//#define DEBUG_LOG_OPENGL_NOTIFICATIONS	// Enable non-essential logging (e.g. OpenGL notifications)
	#endif
	
	// Scene setup and creation logging:
	//----------------------------------
	//#define DEBUG_LOG_SCENEMANAGER_SCENE_SETUP			// Enable/disable scene import logging
	#if defined(DEBUG_LOG_SCENEMANAGER_SCENE_SETUP)
		#define DEBUG_SCENEMANAGER_LIGHT_LOGGING		// Enable logging of light import/creation
		//#define DEBUG_SCENEMANAGER_CAMERA_LOGGING		// Enable logging of camera import/creation
		//#define DEBUG_SCENEMANAGER_MESH_LOGGING			// Enable logging of mesh import/creation
		//#define DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING	// Enable logging of GameObject creation
		//#define DEBUG_SCENEMANAGER_TRANSFORM_LOGGING	// Enable logging of transformation hierarchy setup
		//#define DEBUG_SCENEMANAGER_MATERIAL_LOGGING		// Enable logging of material creation/setup
		//#define DEBUG_SCENEMANAGER_SHADER_LOGGING		// Enable logging of shader creation/setup
		//#define DEBUG_SCENEMANAGER_TEXTURE_LOGGING		// Enable logging of texture creation/setup
	#endif

	//#define DEBUG_LOG_RENDERMANAGER
	#if defined(DEBUG_LOG_RENDERMANAGER)
		#define DEBUG_RENDERMANAGER_SHADER_LOGGING		// Enable logging of shader setup
	#endif

	//#define DEBUG_LOG_SHADERS
	#if defined(DEBUG_LOG_SHADERS)
		#define DEBUG_SHADER_SETUP_LOGGING				// Enable logging of shader loading within the Shader class
		//#define DEBUG_SHADER_PRINT_FINAL_SHADER // Should the final, processed shader be printed? Spews a lot of text!
	#endif

	//#define DEBUG_TRANSFORMS							// Enable transform debugging functions
	//if defined(DEBUG_TRANSFORMS)
		//
	//#endif

#endif