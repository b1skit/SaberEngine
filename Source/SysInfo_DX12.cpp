// © 2023 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "SysInfo_DX12.h"
#include "RenderManager.h"


namespace dx12
{
	uint8_t SysInfo::GetMaxRenderTargets()
	{
		return D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}


	D3D_ROOT_SIGNATURE_VERSION SysInfo::GetHighestSupportedRootSignatureVersion()
	{
		static D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};
		static bool firstQuery(true);
		if (firstQuery)
		{
			firstQuery = false;

			dx12::Context::PlatformParams* ctxPlatParams =
				re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
			ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();
			
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}
		}
		return featureData.HighestVersion;		
	}
}