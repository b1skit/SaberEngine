// © 2022 Adam Badke. All rights reserved.

#include "Config.h"
#include "Debug_DX12.h"
#include "Core\Assert.h"
#include "Device_DX12.h"
#include "RenderManager_DX12.h"

using Microsoft::WRL::ComPtr;
using dx12::CheckHResult;


namespace
{
	constexpr D3D_FEATURE_LEVEL k_featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
		D3D_FEATURE_LEVEL_1_0_CORE
	};


	// Find the display adapter with the highest D3D feature level support, or most VRAM:
	ComPtr<IDXGIAdapter4> GetBestDisplayAdapter()
	{
		// Create a DXGI factory object
		ComPtr<IDXGIFactory4> dxgiFactory;
		uint32_t createFactoryFlags = 0;
#if defined(_DEBUG)
		if (en::Config::Get()->GetValue<int>(en::ConfigKeys::k_debugLevelCmdLineArg) > 0)
		{
			createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
		}
#endif

		HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
		CheckHResult(hr, "Failed to create DXGIFactory2");

		ComPtr<IDXGIAdapter1> dxgiAdapter1;
		ComPtr<IDXGIAdapter4> dxgiAdapter4;

		// Prioritize by highest D3D feature level support, then by amount of VRAM as a tie-breaker. Never choose a 
		// software adapter:
		uint32_t bestFeatureLevelSupportSeen = _countof(k_featureLevels) - 1;
		size_t maxVRAM = 0;
		DXGI_ADAPTER_DESC1 bestAdapterDesc{};

		// Query each of our HW adapters:		
		for (uint32_t i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc{};
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc);

			const size_t vram = dxgiAdapterDesc.DedicatedVideoMemory / (1024u * 1024u);
			LOG(L"Querying adapter %d: %s, %ju MB VRAM", dxgiAdapterDesc.DeviceId, dxgiAdapterDesc.Description, vram);

			uint32_t currentFeatureLevelIdx = 0;
			while (currentFeatureLevelIdx < _countof(k_featureLevels))
			{
				const D3D_FEATURE_LEVEL targetFeatureLevel = k_featureLevels[currentFeatureLevelIdx];

				if ((dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
					SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), targetFeatureLevel, __uuidof(ID3D12Device), nullptr)) &&
					currentFeatureLevelIdx <= bestFeatureLevelSupportSeen &&
					vram > maxVRAM)
				{
					bestFeatureLevelSupportSeen = currentFeatureLevelIdx;
					maxVRAM = vram;
					memcpy(&bestAdapterDesc, &dxgiAdapterDesc, sizeof(dxgiAdapterDesc));

					hr = dxgiAdapter1.As(&dxgiAdapter4);
					CheckHResult(hr, "Failed to cast selected dxgiAdapter4 to dxgiAdapter1");

					break; // Stop checking after we've found the best feature level we can support
				}

				currentFeatureLevelIdx++;
			}
		}

		LOG(L"Selected adapter %d: %s, %ju MB VRAM, %s", 
			bestAdapterDesc.DeviceId, 
			bestAdapterDesc.Description, 
			maxVRAM, 
			util::ToWideString(dx12::GetFeatureLevelAsCStr(k_featureLevels[bestFeatureLevelSupportSeen])).c_str());

		return dxgiAdapter4;
	}


	ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
	{
		ComPtr<ID3D12Device2> device;

		uint32_t featureLevelIdx = 0;
		while (featureLevelIdx < _countof(k_featureLevels))
		{
			HRESULT hr = D3D12CreateDevice(nullptr, k_featureLevels[featureLevelIdx], IID_PPV_ARGS(&device));
			if (SUCCEEDED(hr))
			{
				LOG("Device created for maximum supported D3D feature level: %s", 
					dx12::GetFeatureLevelAsCStr(k_featureLevels[featureLevelIdx]));
				break; // feature level is supported by default adapter
			}
			featureLevelIdx++;
		}

		return device;
	}


	void ConfigureD3DInfoQueue(ComPtr<ID3D12Device> device, uint32_t debugLevel)
	{
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(device.As(&infoQueue)))
		{
			switch (debugLevel)
			{
			case 1:
			{
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			}
			break;
			case 2:
			{
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			}
			break;
			case 3:
			{
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			}
			break;
			default: SEAssertF("Invalid debug level");
			}

			// Suppress message categories
			//D3D12_MESSAGE_CATEGORY Categories[] = {};

			// Suppress messages by severity level
			D3D12_MESSAGE_SEVERITY severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			// Suppress individual messages by ID
			std::vector<D3D12_MESSAGE_ID> denyIds =
			{
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // Intentional usage
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, // Occurs when using capture frame while graphics debugging
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE, // Occurs when using capture frame while graphics debugging
			};

			if (en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false)
			{
				// Empty RTVs in final MIP generation stages
				denyIds.emplace_back(D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET); 
			}

			D3D12_INFO_QUEUE_FILTER newFilter = {};
			//newFilter.DenyList.NumCategories = _countof(Categories);
			//newFilter.DenyList.pCategoryList = Categories;
			newFilter.DenyList.NumSeverities = _countof(severities);
			newFilter.DenyList.pSeverityList = severities;
			newFilter.DenyList.NumIDs = static_cast<uint32_t>(denyIds.size());
			newFilter.DenyList.pIDList = denyIds.data();


			HRESULT hr = infoQueue->PushStorageFilter(&newFilter);
			CheckHResult(hr, "Failed to push storage filter");
		}
	}
}


namespace dx12
{
	Device::Device()
		: m_dxgiAdapter4(nullptr)
		, m_displayDevice(nullptr)
	{
	}


	void Device::Create()
	{
		m_dxgiAdapter4 = GetBestDisplayAdapter(); // Find the display adapter with the most VRAM
		m_displayDevice = CreateDevice(m_dxgiAdapter4); // Create a device from the selected adapter

		const uint32_t debugLevel = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_debugLevelCmdLineArg);
		if (debugLevel > 0)
		{
			ConfigureD3DInfoQueue(m_displayDevice, debugLevel);
		}
	}


	void Device::Destroy()
	{
		m_displayDevice = nullptr;
		m_dxgiAdapter4 = nullptr;
	}
}