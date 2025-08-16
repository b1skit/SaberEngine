// © 2025 Adam Badke. All rights reserved.
#include "DREDHelpers_DX12.h"

#include "Core/Assert.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	DREDQuery DREDQuery::Create(ID3D12Device* device)
	{
		SEAssert(device, "Device cannot be null");

		DREDQuery api;

		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&api.m_dred2))))
		{
			api.m_dred2.As(&api.m_dred1);
			api.m_dred2.As(&api.m_dred);
			api.m_version = D3D12_DRED_VERSION::D3D12_DRED_VERSION_1_2;
			return api;
		}
		else if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&api.m_dred1))))
		{
			api.m_dred1.As(&api.m_dred);
			api.m_version = D3D12_DRED_VERSION::D3D12_DRED_VERSION_1_1;
			return api;
		}
		else if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&api.m_dred))))
		{
			api.m_version = D3D12_DRED_VERSION::D3D12_DRED_VERSION_1_0;
			return api;
		}
		SEAssertF("Could not create a DRED interface");

		return api;
	}

	bool DREDQuery::ForEachBreadcrumb(BreadcrumbCallback const& breadcrumbCallback) const
	{
		if (!IsValid() || breadcrumbCallback == nullptr)
		{
			return false;
		}

		if (m_version >= D3D12_DRED_VERSION_1_1)
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 output{};

			const HRESULT hr = m_dred1->GetAutoBreadcrumbsOutput1(&output);
			if (FAILED(hr))
			{
				return false;
			}

			D3D12_AUTO_BREADCRUMB_NODE1 const* node = output.pHeadAutoBreadcrumbNode;
			while (node != nullptr)
			{
				const DredBreadcrumbNodeView view{
					.m_cmdListNameW = node->pCommandListDebugNameW,
					.m_cmdQueueNameW = node->pCommandQueueDebugNameW,
					.m_lastBreadcrumbValue = node->pLastBreadcrumbValue,
					.m_breadcrumbCount = node->BreadcrumbCount,
					.m_commandHistory = node->pCommandHistory,
					.m_breadcrumbContexts = node->pBreadcrumbContexts,
					.m_breadcrumbContextsCount = node->BreadcrumbContextsCount,
				};
				
				breadcrumbCallback(view);

				node = node->pNext;
			}
		}
		else
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT output{};

			const HRESULT hr = m_dred->GetAutoBreadcrumbsOutput(&output);
			if (FAILED(hr))
			{
				return false;
			}

			D3D12_AUTO_BREADCRUMB_NODE const* node = output.pHeadAutoBreadcrumbNode;
			while (node != nullptr)
			{
				const DredBreadcrumbNodeView view{
					.m_cmdListNameW = node->pCommandListDebugNameW,
					.m_cmdQueueNameW = node->pCommandQueueDebugNameW,
					.m_lastBreadcrumbValue = node->pLastBreadcrumbValue,
					.m_breadcrumbCount = node->BreadcrumbCount,
					.m_commandHistory = node->pCommandHistory,
					.m_breadcrumbContexts = nullptr,
					.m_breadcrumbContextsCount = 0,
				};

				breadcrumbCallback(view);

				node = node->pNext;
			}
		}
		return true;
	}

	bool DREDQuery::GetPageFault(DredPageFaultView& view) const
	{
		if (!IsValid())
		{
			return false;
		}

		if (m_version >= D3D12_DRED_VERSION_1_2)
		{
			D3D12_DRED_PAGE_FAULT_OUTPUT2 output{};
			const HRESULT hr = m_dred2->GetPageFaultAllocationOutput2(&output);
			if (FAILED(hr))
			{
				return false;
			}

			view.m_pageFaultVA = output.PageFaultVA;
			view.m_pageFaultFlags = output.PageFaultFlags;
			view.m_existingHead =
				reinterpret_cast<D3D12_DRED_ALLOCATION_NODE const*>(output.pHeadExistingAllocationNode);
			view.m_recentFreedHead =
				reinterpret_cast<D3D12_DRED_ALLOCATION_NODE const*>(output.pHeadRecentFreedAllocationNode);
		}
		else if (m_version == D3D12_DRED_VERSION_1_1)
		{
			D3D12_DRED_PAGE_FAULT_OUTPUT1 output{};
			const HRESULT hr = m_dred1->GetPageFaultAllocationOutput1(&output);
			if (FAILED(hr))
			{
				return false;
			}

			view.m_pageFaultVA = output.PageFaultVA;
			view.m_pageFaultFlags = D3D12_DRED_PAGE_FAULT_FLAGS_NONE;
			view.m_existingHead =
				reinterpret_cast<const D3D12_DRED_ALLOCATION_NODE*>(output.pHeadExistingAllocationNode);
			view.m_recentFreedHead =
				reinterpret_cast<const D3D12_DRED_ALLOCATION_NODE*>(output.pHeadRecentFreedAllocationNode);
		}
		else
		{
			D3D12_DRED_PAGE_FAULT_OUTPUT output{};
			const HRESULT hr = m_dred->GetPageFaultAllocationOutput(&output);
			if (FAILED(hr))
			{
				return false;
			}

			view.m_pageFaultVA = output.PageFaultVA;
			view.m_pageFaultFlags = D3D12_DRED_PAGE_FAULT_FLAGS_NONE;
			view.m_existingHead = output.pHeadExistingAllocationNode;
			view.m_recentFreedHead = output.pHeadRecentFreedAllocationNode;
		}

		return true;
	}
}

