#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <functional>

namespace dx12
{
        struct DredBreadcrumbNodeView
        {
                const wchar_t* cmdListNameW;
                const wchar_t* cmdQueueNameW;
                const UINT* lastValue;
                UINT count;
                const D3D12_AUTO_BREADCRUMB_OP* history;
                const D3D12_DRED_BREADCRUMB_CONTEXT* contexts;
                UINT contextsCount;
        };

        struct DredPageFaultView
        {
                UINT64 pageFaultVA;
                UINT pageFaultFlags;
                const D3D12_DRED_ALLOCATION_NODE* existingHead;
                const D3D12_DRED_ALLOCATION_NODE* recentFreedHead;
        };

        class DredApi
        {
        public:
                enum class Ver { None, V1, V1_1, V2, V3 };

                static DredApi Query(ID3D12Device* device);

                Ver GetVersion() const { return m_version; }

                using BreadcrumbCallback = std::function<void(DredBreadcrumbNodeView const&)>;

                bool ForEachBreadcrumb(bool* hasContexts, BreadcrumbCallback const& cb) const;
                bool GetPageFault(DredPageFaultView& view) const;

        private:
                Ver m_version = Ver::None;
                Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> m_dred;
                Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData1> m_dred1;
                Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData2> m_dred2;
        #if defined(__ID3D12DeviceRemovedExtendedData3_INTERFACE_DEFINED__)
                Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData3> m_dred3;
        #endif
        };
}

