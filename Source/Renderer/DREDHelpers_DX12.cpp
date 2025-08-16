// © 2025 Adam Badke. All rights reserved.
#include "DREDHelpers_DX12.h"

#include "Core/Assert.h"

#include <algorithm>

using Microsoft::WRL::ComPtr;


namespace dx12
{
	DredApi DredApi::Query(ID3D12Device* device)
	{
		if (!device)
		{
			return DredApi(Ver::None);
		}

		// Try to query DRED interfaces in order from newest to oldest
		// DRED v3 (ID3D12DeviceRemovedExtendedData3)
		ComPtr<ID3D12DeviceRemovedExtendedData3> dred3;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred3))))
		{
			return DredApi(Ver::V3);
		}

		// DRED v2 (ID3D12DeviceRemovedExtendedData2)
		ComPtr<ID3D12DeviceRemovedExtendedData2> dred2;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred2))))
		{
			return DredApi(Ver::V2);
		}

		// DRED v1.1 (ID3D12DeviceRemovedExtendedData1)
		ComPtr<ID3D12DeviceRemovedExtendedData1> dred1_1;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred1_1))))
		{
			return DredApi(Ver::V1_1);
		}

		// DRED v1 (ID3D12DeviceRemovedExtendedData)
		ComPtr<ID3D12DeviceRemovedExtendedData> dred1;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred1))))
		{
			return DredApi(Ver::V1);
		}

		return DredApi(Ver::None);
	}


	bool DredApi::ForEachBreadcrumb(ID3D12Device* device, bool* hasContexts,
		std::function<void(const DredBreadcrumbNodeView&)> callback) const
	{
		if (!device || !hasContexts || !callback || m_version == Ver::None)
		{
			return false;
		}

		*hasContexts = false;

		switch (m_version)
		{
		case Ver::V3:
		case Ver::V2:
		{
			// DRED v2+ uses the same interface as v1 for breadcrumbs
			ComPtr<ID3D12DeviceRemovedExtendedData> dred;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
			{
				return false;
			}

			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbsOutput;
			if (FAILED(dred->GetAutoBreadcrumbsOutput(&breadcrumbsOutput)))
			{
				return false;
			}

			const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbsOutput.pHeadAutoBreadcrumbNode;
			while (node)
			{
				DredBreadcrumbNodeView view;
				view.cmdListNameW = node->pCommandListDebugNameW;
				view.cmdQueueNameW = node->pCommandQueueDebugNameW;
				view.lastValue = node->pLastBreadcrumbValue;
				view.count = node->BreadcrumbCount;
				view.history = node->pCommandHistory;
				view.contexts = nullptr;
				view.contextsCount = 0;

				callback(view);
				node = node->pNext;
			}
			return true;
		}

		case Ver::V1_1:
		{
			// DRED v1.1 adds breadcrumb context support
			ComPtr<ID3D12DeviceRemovedExtendedData1> dred1_1;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred1_1))))
			{
				return false;
			}

			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbsOutput1;
			if (FAILED(dred1_1->GetAutoBreadcrumbsOutput1(&breadcrumbsOutput1)))
			{
				return false;
			}

			const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbsOutput1.pHeadAutoBreadcrumbNode;
			while (node)
			{
				DredBreadcrumbNodeView view;
				view.cmdListNameW = node->pCommandListDebugNameW;
				view.cmdQueueNameW = node->pCommandQueueDebugNameW;
				view.lastValue = node->pLastBreadcrumbValue;
				view.count = node->BreadcrumbCount;
				view.history = node->pCommandHistory;
				view.contexts = node->pBreadcrumbContexts;
				view.contextsCount = node->BreadcrumbContextsCount;

				if (view.contexts && view.contextsCount > 0)
				{
					*hasContexts = true;
				}

				callback(view);
				node = node->pNext;
			}
			return true;
		}

		case Ver::V1:
		{
			// DRED v1 basic functionality
			ComPtr<ID3D12DeviceRemovedExtendedData> dred;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
			{
				return false;
			}

			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbsOutput;
			if (FAILED(dred->GetAutoBreadcrumbsOutput(&breadcrumbsOutput)))
			{
				return false;
			}

			const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbsOutput.pHeadAutoBreadcrumbNode;
			while (node)
			{
				DredBreadcrumbNodeView view;
				view.cmdListNameW = node->pCommandListDebugNameW;
				view.cmdQueueNameW = node->pCommandQueueDebugNameW;
				view.lastValue = node->pLastBreadcrumbValue;
				view.count = node->BreadcrumbCount;
				view.history = node->pCommandHistory;
				view.contexts = nullptr;
				view.contextsCount = 0;

				callback(view);
				node = node->pNext;
			}
			return true;
		}

		default:
			return false;
		}
	}


	bool DredApi::GetPageFault(ID3D12Device* device, DredPageFaultView& pageView) const
	{
		if (!device || m_version == Ver::None)
		{
			return false;
		}

		// Clear the output view
		pageView = {};

		switch (m_version)
		{
		case Ver::V3:
		case Ver::V2:
		{
			// DRED v2+ adds page fault flags
			ComPtr<ID3D12DeviceRemovedExtendedData2> dred2;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred2))))
			{
				return false;
			}

			D3D12_DRED_PAGE_FAULT_OUTPUT2 pageFaultOutput2;
			if (FAILED(dred2->GetPageFaultAllocationOutput2(&pageFaultOutput2)))
			{
				return false;
			}

			pageView.pageFaultVA = pageFaultOutput2.PageFaultVA;
			pageView.pageFaultFlags = pageFaultOutput2.PageFaultFlags;
			pageView.existingHead = pageFaultOutput2.pHeadExistingAllocationNode;
			pageView.recentFreedHead = pageFaultOutput2.pHeadRecentFreedAllocationNode;
			return true;
		}

		case Ver::V1_1:
		case Ver::V1:
		{
			// DRED v1/v1.1 uses basic page fault output
			ComPtr<ID3D12DeviceRemovedExtendedData> dred;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
			{
				return false;
			}

			D3D12_DRED_PAGE_FAULT_OUTPUT pageFaultOutput;
			if (FAILED(dred->GetPageFaultAllocationOutput(&pageFaultOutput)))
			{
				return false;
			}

			pageView.pageFaultVA = pageFaultOutput.PageFaultVA;
			pageView.pageFaultFlags = 0;  // Not available in v1/v1.1
			pageView.existingHead = pageFaultOutput.pHeadExistingAllocationNode;
			pageView.recentFreedHead = pageFaultOutput.pHeadRecentFreedAllocationNode;
			return true;
		}

		default:
			return false;
		}
	}
}