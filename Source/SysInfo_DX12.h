// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>


namespace dx12
{
	class SysInfo
	{
	public: // Common platform:
		static uint8_t GetMaxRenderTargets();


	public: // DX12-specific:		
		static D3D_ROOT_SIGNATURE_VERSION GetHighestSupportedRootSignatureVersion();
		static bool CheckTearingSupport(); // Variable refresh rate dispays (eg. G-Sync/FreeSync) require tearing enabled
	};
}