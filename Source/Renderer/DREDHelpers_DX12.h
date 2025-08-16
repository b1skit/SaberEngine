// © 2025 Adam Badke. All rights reserved.
#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <functional>

namespace dx12
{
	struct DredBreadcrumbNodeView
	{
		const wchar_t* m_cmdListNameW;
		const wchar_t* m_cmdQueueNameW;
		const uint32_t* m_lastBreadcrumbValue;
		uint32_t m_breadcrumbCount;
		const D3D12_AUTO_BREADCRUMB_OP* m_commandHistory;
		const D3D12_DRED_BREADCRUMB_CONTEXT* m_breadcrumbContexts;
		uint32_t m_breadcrumbContextsCount;
	};

	struct DredPageFaultView
	{
		D3D12_GPU_VIRTUAL_ADDRESS m_pageFaultVA;
		D3D12_DRED_PAGE_FAULT_FLAGS m_pageFaultFlags;
		D3D12_DRED_ALLOCATION_NODE const* m_existingHead;
		D3D12_DRED_ALLOCATION_NODE const* m_recentFreedHead;
	};

	class DREDQuery
	{
	public:
		static DREDQuery Create(ID3D12Device* device);

	public:
		bool IsValid() const { return m_version != 0; }
		bool HasContexts() const { return m_version >= D3D12_DRED_VERSION_1_1; }

		using BreadcrumbCallback = std::function<void(DredBreadcrumbNodeView const&)>;
		bool ForEachBreadcrumb(BreadcrumbCallback const&) const;

		bool GetPageFault(DredPageFaultView& view) const;

	private:
		D3D12_DRED_VERSION m_version = static_cast<D3D12_DRED_VERSION>(0);
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> m_dred;
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData1> m_dred1;
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData2> m_dred2;
	};
}

