// © 2023 Adam Badke. All rights reserved.
#pragma once

//#define PIX_USE_GPU_MARKERS_V2
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
#define SEBeginCPUEvent(...) \
	PIXBeginEvent(PIX_COLOR_INDEX(perfmarkers::Type::CPUSection),  __VA_ARGS__);


#define SEEndCPUEvent() \
	PIXEndEvent();


// DX12 GPU markers:
//------------------
#define SEBeginGPUEvent(apiObjPtr, perfMarkerType, ...) \
	PIXBeginEvent( \
		apiObjPtr, \
		PIX_COLOR_INDEX(perfMarkerType), \
		 __VA_ARGS__);


#define SEEndGPUEvent(apiObjPtr) \
	PIXEndEvent(apiObjPtr);


// OpenGL GPU markers:
// Ideally, we'd have a single, unified marker regardless of API. But the glPushDebugGroup/glPopDebugGroup API is far
// simpler than modern APIs. Markers are not tied to API objects, and can't be color-coded. However, we do use the
// perfMarkerType enum as an ID to help identify marker sources
//--------------------
#define SEBeginOpenGLGPUEvent(perfMarkerType, ...) \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, perfMarkerType, -1,  __VA_ARGS__);


#define SEEndOpenGLGPUEvent() \
	glPopDebugGroup();


#else
// Release mode: Remove markers

// CPU markers:
//-------------
#define SEBeginCPUEvent(...) \
	do { static_cast<void>(0, ##__VA_ARGS__); } while(0);


#define SEEndCPUEvent() \
	do {} while(0);


// GPU markers:
//-------------
#define SEBeginGPUEvent(apiObjPtr, perfMarkerType, ...) \
	do { static_cast<void>(apiObjPtr); static_cast<void>(perfMarkerType); static_cast<void>(0, ##__VA_ARGS__); } while(0);


#define SEEndGPUEvent(apiObjPtr) \
	do {} while(0);


#define SEBeginOpenGLGPUEvent(perfMarkerType, ...) \
	do { static_cast<void>(perfMarkerType); static_cast<void>(0, ##__VA_ARGS__); } while(0);


#define SEEndOpenGLGPUEvent() \
	do {} while(0);

#endif


/***********************************************************************************************************************
* Debug marker tracking:
***********************************************************************************************************************/

// DO NOT CHECK THIS IN: Define this to enable debug marker tracking
//#define SE_ENABLE_DEBUG_MARKER_TRACKING

// DO NOT CHECK THIS IN: Convenience helper: Enable this if there is SEEndCPUEventAndVerify macros temporarily in use
//#define TOLERATE_SE_END_EVENT_AND_VERIFY
#if defined(TOLERATE_SE_END_EVENT_AND_VERIFY)
#define SEEndCPUEventAndVerify(...) SEEndCPUEvent()
#endif


#ifdef SE_ENABLE_DEBUG_MARKER_TRACKING

// Include logger:
#include "Logger.h"


// Redefine our macros to use the the tracking functions:
#undef SEBeginCPUEvent
#undef SEEndCPUEvent

#define SEBeginCPUEvent(...) \
	debugperfmarkers::SEInternalBeginCPUEvent(__VA_ARGS__, __FILE__, __LINE__)

#define SEEndCPUEvent() \
	debugperfmarkers::SEInternalEndCPUEvent()

#define SEEndCPUEventAndVerify(...) \
	debugperfmarkers::SEInternalEndCPUEvent(__VA_ARGS__)


// Debug marker tracking implementation:
namespace debugperfmarkers
{

	struct MarkerInfo
	{
		std::string m_name;
		std::string m_file;
		uint32_t m_line;
	};

	inline std::mutex g_mapMutex;
	inline std::unordered_map<std::thread::id, std::stack<MarkerInfo>> g_markerStacks;


	inline void RecordMarkerBegin(const char* m_name, const char* m_file, uint32_t m_line)
	{
		std::lock_guard lock(g_mapMutex);
		auto& threadStack = g_markerStacks[std::this_thread::get_id()];
		threadStack.push({ m_name, m_file, m_line });
	}


	inline void RecordMarkerEnd(char const* name = nullptr)
	{
		std::lock_guard lock(g_mapMutex);
		auto& stack = g_markerStacks[std::this_thread::get_id()];
		if (stack.empty())
		{
			SEAssertF("SEEndCPUEvent() called with no matching SEBeginCPUEvent()");
		}
		else
		{
			if (name != nullptr)
			{
				MarkerInfo const& markerInfo = stack.top();
				SEAssert(std::string(name) == markerInfo.name, "Mismatched marker m_name");
			}
			stack.pop();
		}
	}


	inline void ValidatePerfMarkers()
	{
		std::lock_guard lock(g_mapMutex);

		for (const auto& [threadId, stack] : g_markerStacks)
		{
			if (!stack.empty())
			{
				// Make a copy of the stack so we can compare the contents
				std::stack<MarkerInfo> currentStack = stack;

				while (!currentStack.empty())
				{
					MarkerInfo const& markerInfo = currentStack.top();
					LOG_ERROR(std::format("Leak: {} started at {}:{}", 
						markerInfo.m_name, markerInfo.m_file, markerInfo.m_line).c_str());
					currentStack.pop();
				}
				SEAssertF("Unclosed SEBeginCPUEvent() markers at end of frame");
			}
		}
	}


	inline void SEInternalBeginCPUEvent(char const* m_name, char const* m_file, uint32_t m_line)
	{
		debugperfmarkers::RecordMarkerBegin(m_name, m_file, m_line);
		PIXBeginEvent(PIX_COLOR_INDEX(perfmarkers::Type::CPUSection), m_name);
	}


	inline void SEInternalEndCPUEvent(char const* name = nullptr)
	{
		debugperfmarkers::RecordMarkerEnd(name);
		PIXEndEvent();
	}
} // namespace debugperfmarkers


#endif // SE_ENABLE_DEBUG_MARKER_TRACKING