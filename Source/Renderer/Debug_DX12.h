// © 2022 Adam Badke. All rights reserved.
#pragma once


// Resource barrier debugging macros:
namespace
{
	// Enable any of these:
//#define DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS
//#define DEBUG_CMD_LIST_RESOURCE_TRANSITIONS
//#define DEBUG_CMD_LIST_LOG_STAGE_NAMES // Warning: May overflow maximum allowed log buffer sizes
//#define DEBUG_STATE_TRACKER_RESOURCE_TRANSITIONS


#if defined(DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS) || defined(DEBUG_CMD_LIST_RESOURCE_TRANSITIONS) || defined(DEBUG_STATE_TRACKER_RESOURCE_TRANSITIONS)

	// Enable one or the other of these to filter the output:
//#define FILTER_TRANSITIONS_BY_EXCLUSION
#define FILTER_TRANSITIONS_BY_INCLUSION


	// Add filter words here:
#if defined(FILTER_TRANSITIONS_BY_EXCLUSION)
	constexpr char const* k_excludedNameSubstrings[] = { "Vertex" }; // Case sensitive: Exclude output with these substrings
#define FILTER_NAMES k_excludedNameSubstrings

#elif defined(FILTER_TRANSITIONS_BY_INCLUSION)
	constexpr char const* k_showOnlyNameSubstrings[] = { "GBufferDepth" }; // Case sensitive: Only show output with these substrings
#define FILTER_NAMES k_showOnlyNameSubstrings

#endif //FILTER_TRANSITIONS_BY_EXCLUSION / FILTER_TRANSITIONS_BY_INCLUSION

#endif

	// Returns true if the result should be displayed, false otherwise
	bool ShouldSkipDebugOutput(char const* name)
	{
#if defined(FILTER_TRANSITIONS_BY_EXCLUSION) || defined(FILTER_TRANSITIONS_BY_INCLUSION)
		constexpr size_t k_numNamesToCheck = sizeof(FILTER_NAMES) / sizeof(FILTER_NAMES[0]);
		for (size_t i = 0; i < k_numNamesToCheck; i++)
		{
#if defined(FILTER_TRANSITIONS_BY_EXCLUSION)
			if (strstr(name, FILTER_NAMES[i]) != nullptr)
			{
				return true;
			}
		}
		return false;
#elif defined(FILTER_TRANSITIONS_BY_INCLUSION)
			if (strstr(name, FILTER_NAMES[i]) != nullptr)
			{
				return false;
			}
		}
		return true;
#endif

#else
		return false;
#endif
	}
}


struct ID3D12Object;
enum D3D12_RESOURCE_STATES;
enum D3D_FEATURE_LEVEL;
enum D3D12_RESOURCE_BINDING_TIER;
enum D3D12_RESOURCE_HEAP_TIER;


namespace dx12
{
	extern bool CheckHResult(HRESULT hr, char const* msg);
	extern void EnableDebugLayer();
	extern std::wstring GetWDebugName(ID3D12Object*);
	extern std::string GetDebugName(ID3D12Object*);
	extern constexpr char const* GetResourceStateAsCStr(D3D12_RESOURCE_STATES state);
	extern constexpr char const* GetFeatureLevelAsCStr(D3D_FEATURE_LEVEL);
	extern constexpr char const* D3D12ResourceBindingTierToCStr(D3D12_RESOURCE_BINDING_TIER);
	extern constexpr char const* D3D12ResourceHeapTierToCStr(D3D12_RESOURCE_HEAP_TIER);
}


// Enable this for Aftermath support:
//#define USE_NSIGHT_AFTERMATH
#if defined(USE_NSIGHT_AFTERMATH)

#include "NsightAftermathHelpers.h"
#include "NsightAftermathGpuCrashTracker.h"

namespace aftermath
{
	static class Aftermath
	{
	public:
		Aftermath();

		void InitializeGPUCrashTracker();
		void CreateCommandListContextHandle(ID3D12CommandList*);

		void SetAftermathEventMarker(ID3D12CommandList*, std::string const& markerData, bool appManagedMarker);


	private:
		GpuCrashTracker::MarkerMap m_markerMap; // App-managed marker functionality

		// Nsight Aftermath instrumentation:
		std::unordered_map<ID3D12CommandList const*, GFSDK_Aftermath_ContextHandle> m_aftermathCmdListContexts;
		GpuCrashTracker m_gpuCrashTracker;

		const bool m_isEnabled;

		std::mutex m_aftermathMutex;
	} s_instance;
}

#endif