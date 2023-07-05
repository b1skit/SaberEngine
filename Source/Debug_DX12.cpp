// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

#include "Config.h"
#include "Debug_DX12.h"
#include "DebugConfiguration.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


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
		if (en::Config::Get()->GetValue<int>(en::Config::k_debugLevelCmdLineArg) > 0)
		{
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
			dx12::CheckHResult(hr, "Failed to get debug interface");
			debugInterface->EnableDebugLayer();
		}

		// Enable GPU-based validation for -debuglevel 2 and above:
		if (en::Config::Get()->GetValue<int>(en::Config::k_debugLevelCmdLineArg) > 1)
		{
			ComPtr<ID3D12Debug1> debugInterface1;
			HRESULT hr = debugInterface->QueryInterface(IID_PPV_ARGS(&debugInterface1));
			CheckHResult(hr, "Failed to get query interface");
			debugInterface1->SetEnableGPUBasedValidation(true);
		}
	}
}