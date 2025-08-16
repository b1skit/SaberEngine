#include "DREDHelpers_DX12.h"

using Microsoft::WRL::ComPtr;

namespace dx12
{
        DredApi DredApi::Query(ID3D12Device* device)
        {
                DredApi api;
                if (!device)
                {
                        return api;
                }

        #if defined(__ID3D12DeviceRemovedExtendedData3_INTERFACE_DEFINED__)
                if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&api.m_dred3))))
                {
                        api.m_dred3.As(&api.m_dred2);
                        api.m_dred3.As(&api.m_dred1);
                        api.m_dred3.As(&api.m_dred);
                        api.m_version = Ver::V3;
                        return api;
                }
        #endif
        #if defined(__ID3D12DeviceRemovedExtendedData2_INTERFACE_DEFINED__)
                if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&api.m_dred2))))
                {
                        api.m_dred2.As(&api.m_dred1);
                        api.m_dred2.As(&api.m_dred);
                        api.m_version = Ver::V2;
                        return api;
                }
        #endif
                if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&api.m_dred1))))
                {
                        api.m_dred1.As(&api.m_dred);
                        api.m_version = Ver::V1_1;
                        return api;
                }
                if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&api.m_dred))))
                {
                        api.m_version = Ver::V1;
                        return api;
                }
                return api;
        }

        bool DredApi::ForEachBreadcrumb(bool* hasContexts, BreadcrumbCallback const& cb) const
        {
                if (hasContexts)
                {
                        *hasContexts = (m_version >= Ver::V1_1);
                }

                if (m_version == Ver::None || cb == nullptr)
                {
                        return false;
                }

                if (m_version >= Ver::V1_1)
                {
                        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 output{};
                        HRESULT hr = m_dred1->GetAutoBreadcrumbsOutput1(&output);
                        if (FAILED(hr))
                        {
                                return false;
                        }

                        const D3D12_AUTO_BREADCRUMB_NODE1* node = output.pHeadAutoBreadcrumbNode;
                        while (node != nullptr)
                        {
                                DredBreadcrumbNodeView view{};
                                view.cmdListNameW = node->pCommandListDebugNameW;
                                view.cmdQueueNameW = node->pCommandQueueDebugNameW;
                                view.lastValue = node->pLastBreadcrumbValue;
                                view.count = node->BreadcrumbCount;
                                view.history = node->pCommandHistory;
                                view.contexts = node->pBreadcrumbContexts;
                                view.contextsCount = node->BreadcrumbContextsCount;
                                cb(view);
                                node = node->pNext;
                        }
                }
                else
                {
                        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT output{};
                        HRESULT hr = m_dred->GetAutoBreadcrumbsOutput(&output);
                        if (FAILED(hr))
                        {
                                return false;
                        }
                        const D3D12_AUTO_BREADCRUMB_NODE* node = output.pHeadAutoBreadcrumbNode;
                        while (node != nullptr)
                        {
                                DredBreadcrumbNodeView view{};
                                view.cmdListNameW = node->pCommandListDebugNameW;
                                view.cmdQueueNameW = node->pCommandQueueDebugNameW;
                                view.lastValue = node->pLastBreadcrumbValue;
                                view.count = node->BreadcrumbCount;
                                view.history = node->pCommandHistory;
                                view.contexts = nullptr;
                                view.contextsCount = 0;
                                cb(view);
                                node = node->pNext;
                        }
                }
                return true;
        }

        bool DredApi::GetPageFault(DredPageFaultView& view) const
        {
                if (m_version == Ver::None)
                {
                        return false;
                }

                if (m_version >= Ver::V2)
                {
                        D3D12_DRED_PAGE_FAULT_OUTPUT2 output{};
                        HRESULT hr = m_dred2->GetPageFaultAllocationOutput2(&output);
                        if (FAILED(hr))
                        {
                                return false;
                        }
                        view.pageFaultVA = output.PageFaultVA;
                        view.pageFaultFlags = static_cast<UINT>(output.PageFaultFlags);
                        view.existingHead = reinterpret_cast<const D3D12_DRED_ALLOCATION_NODE*>(output.pHeadExistingAllocationNode);
                        view.recentFreedHead = reinterpret_cast<const D3D12_DRED_ALLOCATION_NODE*>(output.pHeadRecentFreedAllocationNode);
                        return true;
                }
                if (m_version == Ver::V1_1)
                {
                        D3D12_DRED_PAGE_FAULT_OUTPUT1 output{};
                        HRESULT hr = m_dred1->GetPageFaultAllocationOutput1(&output);
                        if (FAILED(hr))
                        {
                                return false;
                        }
                        view.pageFaultVA = output.PageFaultVA;
                        view.pageFaultFlags = 0;
                        view.existingHead = reinterpret_cast<const D3D12_DRED_ALLOCATION_NODE*>(output.pHeadExistingAllocationNode);
                        view.recentFreedHead = reinterpret_cast<const D3D12_DRED_ALLOCATION_NODE*>(output.pHeadRecentFreedAllocationNode);
                        return true;
                }
                D3D12_DRED_PAGE_FAULT_OUTPUT output{};
                HRESULT hr = m_dred->GetPageFaultAllocationOutput(&output);
                if (FAILED(hr))
                {
                        return false;
                }
                view.pageFaultVA = output.PageFaultVA;
                view.pageFaultFlags = 0;
                view.existingHead = output.pHeadExistingAllocationNode;
                view.recentFreedHead = output.pHeadRecentFreedAllocationNode;
                return true;
        }
}

