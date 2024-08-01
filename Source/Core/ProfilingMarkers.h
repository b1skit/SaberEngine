// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Platform.h"

#include "Core/Assert.h"
#include "Core/Config.h"

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

// CPU markers:
//-------------
#define SEBeginCPUEvent(eventNameCStr) \
	PIXBeginEvent(PIX_COLOR_INDEX(perfmarkers::Type::CPUSection), eventNameCStr);


#define SEEndCPUEvent() \
	PIXEndEvent();


// DX12 GPU markers:
//------------------
#define SEBeginGPUEvent(apiObjPtr, perfMarkerType, eventNameCStr) \
	PIXBeginEvent( \
		apiObjPtr, \
		PIX_COLOR_INDEX(perfMarkerType), \
		eventNameCStr);


#define SEEndGPUEvent(apiObjPtr) \
	PIXEndEvent(apiObjPtr);


// OpenGL GPU markers:
// Ideally, we'd have a single, unified marker regardless of API. But the glPushDebugGroup/glPopDebugGroup API is far
// simpler than modern APIs. Markers are not tied to API objects, and can't be color-coded. However, we do use the
// perfMarkerType enum as an ID to help identify marker sources
//--------------------
#define SEBeginOpenGLGPUEvent(perfMarkerType, eventNameCStr) \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, perfMarkerType, -1, eventNameCStr);


#define SEEndOpenGLGPUEvent() \
	glPopDebugGroup();


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
	do { static_cast<void>(apiObjPtr); static_cast<void>(perfMarkerType); static_cast<void>(eventNameCStr); } while(0);


#define SEEndGPUEvent(apiObjPtr) \
	do {} while(0);


#define SEBeginOpenGLGPUEvent(perfMarkerType, eventNameCStr) \
	do { static_cast<void>(perfMarkerType); static_cast<void>(eventNameCStr); } while(0);


#define SEEndOpenGLGPUEvent() \
	do {} while(0);

#endif