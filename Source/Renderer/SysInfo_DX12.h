// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include <d3d12.h>


namespace dx12
{
	class SysInfo
	{
	public: // Common platform:
		static uint8_t GetMaxRenderTargets();
		static uint8_t GetMaxTextureBindPoints();
		static uint8_t GetMaxVertexAttributes();

		static uint32_t GetMaxDescriptorTableCBVs();
		static uint32_t GetMaxDescriptorTableSRVs();
		static uint32_t GetMaxDescriptorTableUAVs();
		

	public: // DX12-specific:		
		static void const* GetD3D12FeatureSupportData(D3D12_FEATURE); // Statically caches query results for reuse

		static D3D_ROOT_SIGNATURE_VERSION GetHighestSupportedRootSignatureVersion();
		static D3D12_RESOURCE_BINDING_TIER GetResourceBindingTier();
		static D3D12_RESOURCE_HEAP_TIER GetResourceHeapTier();
		static D3D12_FEATURE_DATA_ARCHITECTURE1 const* GetFeatureDataArchitecture();

		static uint32_t GetMaxMultisampleQualityLevel(DXGI_FORMAT);

		static bool CheckTearingSupport(); // Variable refresh rate dispays (eg. G-Sync/FreeSync) require tearing enabled
		static uint32_t GetDeviceNodeMask();
		static bool GPUUploadHeapSupported();
	};
}