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


	uint32_t SysInfo::GetMaxConstantBufferViews()
	{
		static const D3D12_RESOURCE_BINDING_TIER resourceBindingTier = 
			static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS const*>(
				GetD3D12FeatureSupportData(D3D12_FEATURE_D3D12_OPTIONS))->ResourceBindingTier;

		// Return limits as specified per the D3D12 hardware tier:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
		switch (resourceBindingTier)
		{
		case D3D12_RESOURCE_BINDING_TIER_1:
		case D3D12_RESOURCE_BINDING_TIER_2:
			return 14;
		case D3D12_RESOURCE_BINDING_TIER_3:
			return 1'000'000; // Full heap
		default: SEAssertF("Invalid resource binding tier");
		}
		return 0; // This should never happen
	}


	uint32_t SysInfo::GetMaxShaderResourceViews()
	{
		static const D3D12_RESOURCE_BINDING_TIER resourceBindingTier =
			static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS const*>(
				GetD3D12FeatureSupportData(D3D12_FEATURE_D3D12_OPTIONS))->ResourceBindingTier;

		// Return limits as specified per the D3D12 hardware tier:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support

		switch (resourceBindingTier)
		{
		case D3D12_RESOURCE_BINDING_TIER_1: 
			return 128;
		case D3D12_RESOURCE_BINDING_TIER_2:
		case D3D12_RESOURCE_BINDING_TIER_3:
			return 1'000'000; // Full heap
		default: SEAssertF("Invalid resource binding tier");
		}
		return 0; // This should never happen
	}


	uint32_t SysInfo::GetMaxUnorderedAccessViews()
	{
		static const D3D12_RESOURCE_BINDING_TIER resourceBindingTier =
			static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS const*>(
				GetD3D12FeatureSupportData(D3D12_FEATURE_D3D12_OPTIONS))->ResourceBindingTier;

		// Return limits as specified per the D3D12 hardware tier:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support

		switch (resourceBindingTier)
		{
		case D3D12_RESOURCE_BINDING_TIER_1:
		{
			static const D3D_FEATURE_LEVEL maxFeatureLevel =
				static_cast<D3D12_FEATURE_DATA_FEATURE_LEVELS const*>(
					GetD3D12FeatureSupportData(D3D12_FEATURE_FEATURE_LEVELS))->MaxSupportedFeatureLevel;
			if (maxFeatureLevel <= D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0)
			{
				return 8;
			}
			return 64;
		}
		break;
		case D3D12_RESOURCE_BINDING_TIER_2: return 64;
		case D3D12_RESOURCE_BINDING_TIER_3: return 1'000'000; // Full heap
		default: SEAssertF("Invalid resource binding tier");
		}
		return 0; // This should never happen
	}


	void const* SysInfo::GetD3D12FeatureSupportData(D3D12_FEATURE d3d12Feature)
	{
		static std::mutex firstQueryMutex;

		switch (d3d12Feature)
		{
		case D3D12_FEATURE_D3D12_OPTIONS:
		{
			static D3D12_FEATURE_DATA_D3D12_OPTIONS optionsData{};
			static bool firstQuery(true);
			if (firstQuery)
			{
				std::lock_guard<std::mutex> lock(firstQueryMutex);

				if (firstQuery)
				{
					const HRESULT hr =
						re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CheckFeatureSupport(
							D3D12_FEATURE_D3D12_OPTIONS,
							&optionsData,
							sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS));
					CheckHResult(hr, "Failed to check D3D12_FEATURE_DATA_D3D12_OPTIONS support");

					firstQuery = false;
				}
			}
			return &optionsData;
		}
		break;
		case D3D12_FEATURE_FEATURE_LEVELS:
		{
			constexpr D3D_FEATURE_LEVEL k_featureLevels[] = { 
				D3D_FEATURE_LEVEL_1_0_GENERIC, 
				D3D_FEATURE_LEVEL_1_0_CORE, 
				D3D_FEATURE_LEVEL_9_1, 
				D3D_FEATURE_LEVEL_9_2, 
				D3D_FEATURE_LEVEL_9_3, 
				D3D_FEATURE_LEVEL_10_0, 
				D3D_FEATURE_LEVEL_10_1, 
				D3D_FEATURE_LEVEL_11_0, 
				D3D_FEATURE_LEVEL_11_1, 
				D3D_FEATURE_LEVEL_12_0, 
				D3D_FEATURE_LEVEL_12_1, 
				D3D_FEATURE_LEVEL_12_2, };

			static D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels{
				.NumFeatureLevels = _countof(k_featureLevels), 
				.pFeatureLevelsRequested = k_featureLevels,
				.MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_12_2 };

			static bool firstQuery(true);
			if (firstQuery)
			{
				std::lock_guard<std::mutex> lock(firstQueryMutex);

				if (firstQuery)
				{
					const HRESULT hr =
						re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CheckFeatureSupport(
							D3D12_FEATURE_FEATURE_LEVELS,
							&featureLevels,
							sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS));
					CheckHResult(hr, "Failed to check D3D12_FEATURE_FEATURE_LEVELS support");

					firstQuery = false;
				}
			}
			return &featureLevels;
		}
		break;
		case D3D12_FEATURE_FORMAT_SUPPORT:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS:
		{
			SEAssertF("This function is implemented elsewhere");
		}
		break;
		case D3D12_FEATURE_FORMAT_INFO:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_SHADER_MODEL:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS1:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_SUPPORT:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_ROOT_SIGNATURE:
		{
			static D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ .HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_2, };
			static bool firstQuery(true);
			if (firstQuery)
			{
				std::lock_guard<std::mutex> lock(firstQueryMutex);

				if (firstQuery)
				{
					ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

					while(featureData.HighestVersion != D3D_ROOT_SIGNATURE_VERSION_1)
					{
						if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
						{
							featureData.HighestVersion = 
								static_cast<D3D_ROOT_SIGNATURE_VERSION>(static_cast<uint8_t>(featureData.HighestVersion) - 1);
						}
						else
						{
							break;
						}
					}

					firstQuery = false;
				}
			}
			return &featureData;
		}
		break;
		case D3D12_FEATURE_ARCHITECTURE:
		case D3D12_FEATURE_ARCHITECTURE1:
		{
			static D3D12_FEATURE_DATA_ARCHITECTURE1 architectureData{ .NodeIndex = 0, }; // Always node 0 for now...
			static bool firstQuery(true);
			if (firstQuery)
			{
				std::lock_guard<std::mutex> lock(firstQueryMutex);

				if (firstQuery)
				{
					const HRESULT hr =
						re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CheckFeatureSupport(
							D3D12_FEATURE_ARCHITECTURE1,
							&architectureData,
							sizeof(D3D12_FEATURE_DATA_ARCHITECTURE1));
					CheckHResult(hr, "Failed to check D3D12_FEATURE_DATA_ARCHITECTURE1 support");

					firstQuery = false;
				}
			}
			return &architectureData;
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS2:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_SHADER_CACHE:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_COMMAND_QUEUE_PRIORITY:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS3:
		{
			static D3D12_FEATURE_DATA_D3D12_OPTIONS3 optionsData{};
			static bool firstQuery(true);
			if (firstQuery)
			{
				std::lock_guard<std::mutex> lock(firstQueryMutex);
				if (firstQuery)
				{
					const HRESULT hr =
						re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CheckFeatureSupport(
							D3D12_FEATURE_D3D12_OPTIONS3,
							&optionsData,
							sizeof(D3D12_FEATURE_DATA_ARCHITECTURE1));
					CheckHResult(hr, "Failed to check D3D12_FEATURE_DATA_ARCHITECTURE1 support");

					firstQuery = false;
				}
			}
			return &optionsData;
		}
		break;
		case D3D12_FEATURE_EXISTING_HEAPS:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS4:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_SERIALIZATION:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_CROSS_NODE:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS5:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_DISPLAYABLE:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS6:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_QUERY_META_COMMAND:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS7:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_TYPE_COUNT:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_TYPES:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS8:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS9:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS10:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS11:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS12:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS13:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS14:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS15:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS16:
		{
			static D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16{};
			static bool firstQuery(true);
			if (firstQuery)
			{
				std::lock_guard<std::mutex> lock(firstQueryMutex);

				if (firstQuery)
				{
					const HRESULT hr = 
						re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()->CheckFeatureSupport(
							D3D12_FEATURE_D3D12_OPTIONS16,
							&options16,
							sizeof(options16));
					CheckHResult(hr, "Failed to check D3D12_FEATURE_D3D12_OPTIONS16 support");

					firstQuery = false;
				}
			}
			return &options16;
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS17:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS18:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS19:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_D3D12_OPTIONS20:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_PREDICATION:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_PLACED_RESOURCE_SUPPORT_INFO:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		case D3D12_FEATURE_HARDWARE_COPY:
		{
			SEAssertF("TODO: Implement support for this query");
		}
		break;
		default: SEAssertF("Invalid D3D12_FEATURE");
		}
		return nullptr; // This should never happen
	}
	

	D3D_ROOT_SIGNATURE_VERSION SysInfo::GetHighestSupportedRootSignatureVersion()
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE const* featureData = 
			static_cast<D3D12_FEATURE_DATA_ROOT_SIGNATURE const*>(GetD3D12FeatureSupportData(D3D12_FEATURE_ROOT_SIGNATURE));

		return featureData->HighestVersion;
	}


	D3D12_RESOURCE_BINDING_TIER SysInfo::GetResourceBindingTier()
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS const* featureData =
			static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS const*>(GetD3D12FeatureSupportData(D3D12_FEATURE_D3D12_OPTIONS));

		return featureData->ResourceBindingTier;
	}


	D3D12_RESOURCE_HEAP_TIER SysInfo::GetResourceHeapTier()
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS const* featureData =
			static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS const*>(GetD3D12FeatureSupportData(D3D12_FEATURE_D3D12_OPTIONS));

		return featureData->ResourceHeapTier;
	}


	D3D12_FEATURE_DATA_ARCHITECTURE1 const* SysInfo::GetFeatureDataArchitecture()
	{
		D3D12_FEATURE_DATA_ARCHITECTURE1 const* architectureData =
			static_cast<D3D12_FEATURE_DATA_ARCHITECTURE1 const*>(GetD3D12FeatureSupportData(D3D12_FEATURE_ARCHITECTURE1));

		return architectureData;
	}


	uint32_t SysInfo::GetMaxMultisampleQualityLevel(DXGI_FORMAT format)
	{
		if (format == DXGI_FORMAT_UNKNOWN)
		{
			return 0;
		}

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		uint32_t sampleCount = 16;

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multisampleQualityLevels{
			.Format = format,
			.SampleCount = sampleCount,
			.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, // Note: We currently ignore tiled resources...
		};

		while (sampleCount >= 1)
		{
			if (!FAILED(device->CheckFeatureSupport(
					D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
					&multisampleQualityLevels,
					sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS))) &&
				multisampleQualityLevels.NumQualityLevels > 0)
			{
				return multisampleQualityLevels.NumQualityLevels;				
			}
			--sampleCount;
		}
		
		return 0;
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
		D3D12_FEATURE_DATA_D3D12_OPTIONS16 const* options16 = 
			static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS16 const*>(GetD3D12FeatureSupportData(D3D12_FEATURE_D3D12_OPTIONS16));

		return options16->GPUUploadHeapSupported;
	}
}