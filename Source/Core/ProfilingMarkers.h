// � 2023 Adam Badke. All rights reserved.
#pragma once

// Enable PIX v2 GPU markers for better compatibility with external debugging tools (PIX, NSight Graphics)
// PIX v2 markers provide more stable frame capture replay and better integration with debugging tools
// Uncomment the line below to completely disable PIX markers if compatibility issues persist
//#define DISABLE_PIX_MARKERS_FOR_EXTERNAL_TOOLS
#define PIX_USE_GPU_MARKERS_V2
#include <pix3.h>


namespace perfmarkers
{
	// This enum allows us to use consistent names/values, so PIX can assign an arbitrary color via the 
	// PIX_COLOR_INDEX(BYTE i) macro
	enum Type : uint8_t
	{
		CPUSection,
		
		CopyQueue,
		CopyCommandList,
		
		GraphicsQueue,
		GraphicsCommandList,

		ComputeQueue,
		ComputeCommandList
	};
}

/***********************************************************************************************************************
*	Notes:
*	- Event names are expected to be null-terminated C-strings
***********************************************************************************************************************/


#if defined(_DEBUG) || defined(PROFILE)
// Debug/Profile mode: Markers enabled

#if defined(DISABLE_PIX_MARKERS_FOR_EXTERNAL_TOOLS)
// Emergency fallback: Disable all markers if external tools have compatibility issues

// CPU markers:
//-------------
#define SEBeginCPUEvent(eventNameCStr) \
	do { static_cast<void>(eventNameCStr); } while(0)

#define SEEndCPUEvent() \
	do {} while(0)

// DX12 GPU markers:
//------------------
#define SEBeginGPUEvent(apiObjPtr, perfMarkerType, eventNameCStr) \
	do { \
		static_cast<void>(apiObjPtr); \
		static_cast<void>(perfMarkerType); \
		static_cast<void>(eventNameCStr); \
	} while(0)

#define SEEndGPUEvent(apiObjPtr) \
	do { \
		static_cast<void>(apiObjPtr); \
	} while(0)

// OpenGL GPU markers:
//--------------------
#define SEBeginOpenGLGPUEvent(perfMarkerType, eventNameCStr) \
	do { \
		static_cast<void>(perfMarkerType); \
		static_cast<void>(eventNameCStr); \
	} while(0)

#define SEEndOpenGLGPUEvent() \
	do {} while(0)

#else
// Normal debug mode: PIX markers enabled

// CPU markers:
//-------------
#define SEBeginCPUEvent(eventNameCStr) \
	PIXBeginEvent(PIX_COLOR_INDEX(perfmarkers::Type::CPUSection), eventNameCStr);


#define SEEndCPUEvent() \
	PIXEndEvent();


// DX12 GPU markers:
//------------------
#define SEBeginGPUEvent(apiObjPtr, perfMarkerType, eventNameCStr) \
	do { \
		if (apiObjPtr != nullptr && eventNameCStr != nullptr) { \
			PIXBeginEvent( \
				apiObjPtr, \
				PIX_COLOR_INDEX(perfMarkerType), \
				eventNameCStr); \
		} \
	} while(0)


#define SEEndGPUEvent(apiObjPtr) \
	do { \
		if (apiObjPtr != nullptr) { \
			PIXEndEvent(apiObjPtr); \
		} \
	} while(0)


// OpenGL GPU markers:
// Ideally, we'd have a single, unified marker regardless of API. But the glPushDebugGroup/glPopDebugGroup API is far
// simpler than modern APIs. Markers are not tied to API objects, and can't be color-coded. However, we do use the
// perfMarkerType enum as an ID to help identify marker sources
//--------------------
#define SEBeginOpenGLGPUEvent(perfMarkerType, eventNameCStr) \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, perfMarkerType, -1, eventNameCStr);


#define SEEndOpenGLGPUEvent() \
	glPopDebugGroup();

#endif // DISABLE_PIX_MARKERS_FOR_EXTERNAL_TOOLS

#else
// Release mode: Remove markers

// CPU markers:
//-------------
#define SEBeginCPUEvent(eventNameCStr) \
	do { static_cast<void>(eventNameCStr); } while(0);


#define SEEndCPUEvent() \
	do {} while(0);


// GPU markers:
//-------------
#define SEBeginGPUEvent(apiObjPtr, perfMarkerType, eventNameCStr) \
	do { \
		static_cast<void>(apiObjPtr); \
		static_cast<void>(perfMarkerType); \
		static_cast<void>(eventNameCStr); \
	} while(0)


#define SEEndGPUEvent(apiObjPtr) \
	do { \
		static_cast<void>(apiObjPtr); \
	} while(0)


#define SEBeginOpenGLGPUEvent(perfMarkerType, eventNameCStr) \
	do { static_cast<void>(perfMarkerType); static_cast<void>(eventNameCStr); } while(0);


#define SEEndOpenGLGPUEvent() \
	do {} while(0);

#endif