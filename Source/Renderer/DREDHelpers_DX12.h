// © 2025 Adam Badke. All rights reserved.
#pragma once

#include <functional>


// Forward declarations
struct ID3D12Device;
enum D3D12_AUTO_BREADCRUMB_OP;
struct D3D12_DRED_BREADCRUMB_CONTEXT;
struct D3D12_DRED_ALLOCATION_NODE;


namespace dx12
{
	// Normalized view of breadcrumb node data across all DRED versions
	struct DredBreadcrumbNodeView
	{
		const wchar_t* cmdListNameW = nullptr;
		const wchar_t* cmdQueueNameW = nullptr;
		const UINT* lastValue = nullptr;
		UINT count = 0;
		const D3D12_AUTO_BREADCRUMB_OP* history = nullptr;
		const D3D12_DRED_BREADCRUMB_CONTEXT* contexts = nullptr;
		UINT contextsCount = 0;
	};


	// Normalized view of page fault data across all DRED versions
	struct DredPageFaultView
	{
		UINT64 pageFaultVA = 0;
		UINT pageFaultFlags = 0;  // Only available in v2+
		const D3D12_DRED_ALLOCATION_NODE* existingHead = nullptr;
		const D3D12_DRED_ALLOCATION_NODE* recentFreedHead = nullptr;
	};


	// DRED API version abstraction layer
	class DredApi
	{
	public:
		enum Ver
		{
			None,
			V1,
			V1_1,
			V2,
			V3
		};

		// Query the highest supported DRED version for the given device
		static DredApi Query(ID3D12Device* device);

		// Iterate through all breadcrumb nodes with normalized view
		// Returns true if successful, false if no breadcrumb data available
		// hasContexts will be set to true if any breadcrumb contexts are available
		bool ForEachBreadcrumb(ID3D12Device* device, bool* hasContexts, 
			std::function<void(const DredBreadcrumbNodeView&)> callback) const;

		// Get page fault information with normalized view
		// Returns true if successful, false if no page fault data available
		bool GetPageFault(ID3D12Device* device, DredPageFaultView& pageView) const;

		// Get the detected DRED version
		Ver GetVersion() const { return m_version; }

	private:
		DredApi(Ver version) : m_version(version) {}
		Ver m_version = None;
	};
}