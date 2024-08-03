// © 2022 Adam Badke. All rights reserved.
#pragma once

struct ID3D12Object;
enum D3D12_RESOURCE_STATES;
enum D3D_FEATURE_LEVEL;

// Resource barrier debugging macros:
namespace
{
	// Enable any of these:
//#define DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS
//#define DEBUG_CMD_LIST_RESOURCE_TRANSITIONS
//#define DEBUG_CMD_LIST_LOG_STAGE_NAMES
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


namespace dx12
{
	inline extern bool CheckHResult(HRESULT hr, char const* msg);
	inline extern void EnableDebugLayer();
	inline extern std::wstring GetWDebugName(ID3D12Object*);
	inline extern std::string GetDebugName(ID3D12Object*);
	inline extern constexpr char const* GetResourceStateAsCStr(D3D12_RESOURCE_STATES state);
	inline extern constexpr char const* GetFeatureLevelAsCStr(D3D_FEATURE_LEVEL);
}