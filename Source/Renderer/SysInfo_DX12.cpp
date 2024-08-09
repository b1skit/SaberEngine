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


	uint8_t SysInfo::GetMaxTextureBindPoints()
	{
		// The DX12 resource binding model allows arbitrary numbers of binding points via descriptor tables. We
		// (currently) maintain this function to ensure parity with OpenGL, and just return an arbitrary large but sane
		// value here
		constexpr uint8_t k_maxTexBindPoints = 32;
		return k_maxTexBindPoints;
	}


	uint8_t SysInfo::GetMaxVertexAttributes()
	{
		return D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
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


	uint32_t SysInfo::GetDeviceNodeMask()
	{
		return 0; // Always 0: We don't (currently) support multiple GPUs
	}


	bool SysInfo::GPUUploadHeapSupported()
	{
		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16{};

		const HRESULT hr = device->CheckFeatureSupport(
			D3D12_FEATURE_D3D12_OPTIONS16,
			&options16,
			sizeof(options16));
		CheckHResult(hr, "Failed to check feature support");

		return options16.GPUUploadHeapSupported;
	}
}