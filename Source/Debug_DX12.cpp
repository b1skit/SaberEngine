// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

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
		{
			SEAssertF("S_FALSE is a success code. Use the SUCCEEDED or FAILED macros instead of calling this function");
		}
		break;
		case E_INVALIDARG:
		{
			LOG_ERROR("%s: One or more arguments are invalid", msg);
			SEAssertF(msg);
		}
		break;
		default:
		{
			LOG_ERROR(msg);
			throw std::exception();
		}
		}

		// Throw an exception here; asserts are disabled in release mode
		throw std::exception();
		return false;
	}


	void EnableDebugLayer()
	{
		// Enable the debug layer in debug build configuration to catch any errors generated while creating DX12 objects
#if defined(_DEBUG)
		ComPtr<ID3D12Debug> debugInterface;
		const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)); // IID_PPV_ARGS macro supplies the RIID & interface pointer
		dx12::CheckHResult(hr, "Failed to enable debug layer");
		debugInterface->EnableDebugLayer();
#endif
	}
}