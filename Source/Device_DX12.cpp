// © 2022 Adam Badke. All rights reserved.

#include "Debug_DX12.h"
#include "DebugConfiguration.h"
#include "Device_DX12.h"
#include "RenderManager_DX12.h"


namespace
{
	using Microsoft::WRL::ComPtr;
	using dx12::CheckHResult;


	// Find the display adapter with the most VRAM:
	ComPtr<IDXGIAdapter4> GetBestDisplayAdapter()
	{
		// Create a DXGI factory object
		ComPtr<IDXGIFactory4> dxgiFactory;
		uint32_t createFactoryFlags = 0;
#if defined(_DEBUG)
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

		HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
		CheckHResult(hr, "Failed to create DXGIFactory2");

		ComPtr<IDXGIAdapter1> dxgiAdapter1;
		ComPtr<IDXGIAdapter4> dxgiAdapter4;

		// Query each of our HW adapters:
		size_t maxVRAM = 0;
		for (uint32_t i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			const size_t vram = dxgiAdapterDesc1.DedicatedVideoMemory / (1024u * 1024u);
			LOG(L"Querying adapter %d: %s, %ju MB VRAM", dxgiAdapterDesc1.DeviceId, dxgiAdapterDesc1.Description, vram);

			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(
						dxgiAdapter1.Get(), dx12::RenderManager::k_targetFeatureLevel, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxVRAM)
			{
				maxVRAM = dxgiAdapterDesc1.DedicatedVideoMemory;

				hr = dxgiAdapter1.As(&dxgiAdapter4);
				CheckHResult(hr, "Failed to cast selected dxgiAdapter4 to dxgiAdapter1");
			}
		}

		return dxgiAdapter4;
	}


	ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
	{
		ComPtr<ID3D12Device2> d3d12Device2;
		HRESULT hr = D3D12CreateDevice(adapter.Get(), dx12::RenderManager::k_targetFeatureLevel, IID_PPV_ARGS(&d3d12Device2));
		CheckHResult(hr, "Failed to create device");

#if defined(_DEBUG)
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(d3d12Device2.As(&infoQueue)))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			// Suppress message categories
			//D3D12_MESSAGE_CATEGORY Categories[] = {};

			// Suppress messages by severity level
			D3D12_MESSAGE_SEVERITY severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			// Suppress individual messages by ID
			D3D12_MESSAGE_ID denyIds[] =
			{
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,	// No idea how to avoid this message yet
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,		// Occurs when using capture frame while graphics debugging
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,	// Occurs when using capture frame while graphics debugging
			};

			D3D12_INFO_QUEUE_FILTER newFilter = {};
			//NewFilter.DenyList.NumCategories = _countof(Categories);
			//NewFilter.DenyList.pCategoryList = Categories;
			newFilter.DenyList.NumSeverities = _countof(severities);
			newFilter.DenyList.pSeverityList = severities;
			newFilter.DenyList.NumIDs = _countof(denyIds);
			newFilter.DenyList.pIDList = denyIds;

			hr = infoQueue->PushStorageFilter(&newFilter);
			CheckHResult(hr, "Failed to push storage filter");
		}
#endif

		return d3d12Device2;
	}
}


namespace dx12
{
	void Device::Create()
	{
		m_dxgiAdapter4 = GetBestDisplayAdapter(); // Find the display adapter with the most VRAM
		m_displayDevice = CreateDevice(m_dxgiAdapter4); // Create a device from the selected adapter

		m_fence.Create();
	}


	void Device::Destroy()
	{
		m_fence.Destroy();

		m_displayDevice = nullptr;
		m_dxgiAdapter4 = nullptr;
	}
}