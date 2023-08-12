// © 2023 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "SysInfo_DX12.h"
#include "RenderManager.h"

using Microsoft::WRL::ComPtr;


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

			ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();
			
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}
		}
		return featureData.HighestVersion;		
	}


	bool SysInfo::CheckTearingSupport()
	{
		int allowTearing = 0;

		ComPtr<IDXGIFactory5> factory5;

		HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&factory5));
		CheckHResult(hr, "Failed to create DXGI Factory");

		hr = factory5->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&allowTearing,
			sizeof(allowTearing));
		CheckHResult(hr, "Failed to check feature support");

		return allowTearing > 0;
	}
}