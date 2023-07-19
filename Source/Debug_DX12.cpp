// � 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <d3d12.h>
#include <wrl.h>

#include "Config.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "DebugConfiguration.h"

using Microsoft::WRL::ComPtr;


namespace
{
	void HandleDRED()
	{
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
		SEAssert("Failed to get DRED query interface", 
			SUCCEEDED(ctxPlatParams->m_device.GetD3DDisplayDevice()->QueryInterface(IID_PPV_ARGS(&pDred))));

		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
		SEAssert("Failed to get DRED auto breadcrumbs output", 
			SUCCEEDED(pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput)));
		
		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		SEAssert("Failed to get DRED page fault allocation output",
			SUCCEEDED(pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput)));
		
		// WIP: Easier to do this when I encounter a DRED
		// https://devblogs.microsoft.com/directx/dred/
		SEAssertF("TODO: Process/output DRED data here");
	}
}

namespace dx12
{
	bool CheckHResult(HRESULT hr, char const* msg)
	{
		switch (hr)
		{
		case S_OK:
		{
			return true;
		}
		break;
		case S_FALSE:
		case DXGI_STATUS_OCCLUDED: // 0x087a0001
		{
			SEAssertF(
				"Checked HRESULT of a success code. Use the SUCCEEDED or FAILED macros instead of calling this function");
		}
		break;
		case DXGI_ERROR_DEVICE_REMOVED: LOG_ERROR("%s: Device removed", msg); break;
		case E_ABORT:					LOG_ERROR("%s: Operation aborted", msg); break;
		case E_ACCESSDENIED:			LOG_ERROR("%s: General access denied error", msg); break;
		case E_FAIL:					LOG_ERROR("%s: Unspecified failure", msg); break;
		case E_HANDLE:					LOG_ERROR("%s: Handle that is not valid", msg); break;
		case E_INVALIDARG:				LOG_ERROR("%s: One or more arguments are invalid", msg); break;
		case E_NOINTERFACE:				LOG_ERROR("%s: No such interface supported", msg); break;
		case E_NOTIMPL:					LOG_ERROR("%s: Not implemented", msg); break;
		case E_OUTOFMEMORY:				LOG_ERROR("%s: Failed to allocate necessary memory", msg); break;
		case E_POINTER:					LOG_ERROR("%s: Pointer that is not valid", msg); break;
		case E_UNEXPECTED:				LOG_ERROR("%s: Unexpected failure", msg); break;
		case ERROR_FILE_NOT_FOUND:		LOG_ERROR("File not found: %s", msg); break;
		default:
		{
			LOG_ERROR(msg);
		}
		}

		// DRED reporting:
		if (hr == DXGI_ERROR_DEVICE_REMOVED && 
			en::Config::Get()->GetValue<int>(en::Config::k_debugLevelCmdLineArg) >= 3)
		{
			HandleDRED();
		}

#if defined(_DEBUG)
		SEAssertF(msg);
#else
		throw std::exception(); // Throw an exception here; asserts are disabled in release mode
#endif

		return false;
	}


	void EnableDebugLayer()
	{
		ComPtr<ID3D12Debug> debugInterface;

		// Enable the debug layer for debuglevel 1 and above:
		if (en::Config::Get()->GetValue<int>(en::Config::k_debugLevelCmdLineArg) >= 1)
		{
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
			dx12::CheckHResult(hr, "Failed to get debug interface");
			debugInterface->EnableDebugLayer();
		}

		// Enable GPU-based validation for -debuglevel 2 and above:
		if (en::Config::Get()->GetValue<int>(en::Config::k_debugLevelCmdLineArg) >= 2)
		{
			ComPtr<ID3D12Debug1> debugInterface1;
			HRESULT hr = debugInterface->QueryInterface(IID_PPV_ARGS(&debugInterface1));
			CheckHResult(hr, "Failed to get query interface");
			debugInterface1->SetEnableGPUBasedValidation(true);
		}

		if (en::Config::Get()->GetValue<int>(en::Config::k_debugLevelCmdLineArg) >= 3)
		{
			ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings));
			CheckHResult(hr, "Failed to get DRED interface");

			// Turn on AutoBreadcrumbs and Page Fault reporting
			dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		}
	}


	std::wstring GetWDebugName(ID3D12Object* object)
	{
		// Name our descriptor heap. We extract the command list's debug name to ensure consistency
		constexpr uint32_t k_nameLength = 1024;
		uint32_t nameLength = k_nameLength;
		wchar_t extractedname[k_nameLength];
		object->GetPrivateData(WKPDID_D3DDebugObjectNameW, &nameLength, &extractedname);
		SEAssert("Invalid name length retrieved", nameLength > 0);

		extractedname[k_nameLength - 1] = '\0'; // Suppress warning C6054

		return std::wstring(extractedname);
	}


	std::string GetDebugName(ID3D12Object* object)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		return converter.to_bytes(GetWDebugName(object));
	}
}