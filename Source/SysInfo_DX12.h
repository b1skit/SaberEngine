// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>


namespace dx12
{
	class SysInfo
	{
	public:
		static D3D_ROOT_SIGNATURE_VERSION GetHighestSupportedRootSignatureVersion();
	};
}