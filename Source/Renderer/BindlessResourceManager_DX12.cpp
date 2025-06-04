// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager.h"
#include "BindlessResourceManager_DX12.h"
#include "Context_DX12.h"
#include "RenderManager.h"
#include "RootSignature_DX12.h"
#include "SysInfo_DX12.h"

#include "Shaders/Common/CameraParams.h"
#include "Shaders/Common/MaterialParams.h"
#include "Shaders/Common/RayTracingParams.h"
#include "Shaders/Common/TransformParams.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <dxgiformat.h>


namespace
{
	constexpr D3D12_DESCRIPTOR_HEAP_TYPE k_brmHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateShaderVisibleDescriptorHeaps(
		ID3D12Device* device, uint32_t numDescriptors, uint8_t frameIdx)
	{
		// Create our  descriptor heap:
		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{
			.Type = k_brmHeapType,
			.NumDescriptors = numDescriptors,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask()
		};

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> newDescriptorHeap;

		dx12::CheckHResult(
			device->CreateDescriptorHeap(
				&descriptorHeapDesc,
				IID_PPV_ARGS(&newDescriptorHeap)),
			"Failed to create descriptor heap");

		newDescriptorHeap->SetName(
			util::ToWideString(std::format("BindlessResourceManager GPU descriptor heap #{}", frameIdx)).c_str());

		return newDescriptorHeap;
	}


	std::unique_ptr<dx12::RootSignature> CreateGlobalBRMRootSignature()
	{
		// Create a global root signature:
		std::unique_ptr<dx12::RootSignature> globalRootSig = dx12::RootSignature::CreateUninitialized();

		std::vector<dx12::RootSignature::DescriptorRangeCreateDesc> tableRanges;
		tableRanges.reserve(dx12::RootSignature::k_maxRootSigEntries);

		// Bindless resources are overlapped using register spaces. We reserve the first 20 register spaces for
		// shader-specific resources
		constexpr uint32_t k_firstBindlessRegisterSpace = 20;

		// CBV Buffers:
		//-------------
		uint32_t cbvRegisterSpace = k_firstBindlessRegisterSpace;

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = CameraData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = cbvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = TraceRayData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = cbvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = DescriptorIndexData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = cbvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
		});


		// SRV Buffers:
		//-------------
		uint32_t srvRegisterSpace = k_firstBindlessRegisterSpace;

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = VertexStreamLUTData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});


		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = InstancedBufferLUTData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = TransformData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = PBRMetallicRoughnessData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = UnlitData::s_shaderName,
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});


		// SRV RaytracingAccelerationStructure:
		//-------------------------------------
		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "SceneBVH",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
			}
		});


		// SRV Textures:
		//--------------
		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "Texture2DFloat4",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "Texture2DFloat",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "Texture2DUint",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32_UINT,
				.m_viewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "Texture2DArrayFloat",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "TextureCubeFloat4",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "TextureCubeArrayFloat",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY,
			}
		});

		// SRV Vertex streams:
		//--------------------
		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "VertexStreams_UShort",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R16_UINT,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "VertexStreams_UInt",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32_UINT,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "VertexStreams_Float2",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32G32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "VertexStreams_Float3",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32G32B32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "VertexStreams_Float4",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = srvRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});


		// UAV Textures:
		//--------------
		uint32_t uavRegisterSpace = k_firstBindlessRegisterSpace;

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "Texture2DRWFloat4",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = uavRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_uavDesc = {
				.m_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.m_viewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
			}
		});

		// Add our overlapping ranges as a single descriptor table:
		globalRootSig->AddDescriptorTable(tableRanges, D3D12_SHADER_VISIBILITY_ALL);


		// For now, we only use bindless resources in DXR, so we hard-code the root signature to match.
		// TODO: Generalize the root signature creation (or define it directly in HLSL) so we can use bindless resources
		// in any/all shaders
		constexpr uint32_t k_firstReservedSpaceIdx = 0;

		// Root constant:
		globalRootSig->AddRootParameter(dx12::RootSignature::RootParameterCreateDesc{
			.m_shaderName = "GlobalConstants",
			.m_type = dx12::RootSignature::RootParameter::Type::Constant,
			.m_registerBindPoint = 0,
			.m_registerSpace = k_firstReservedSpaceIdx,
			.m_flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
			.m_visibility = D3D12_SHADER_VISIBILITY_ALL,
			.m_numRootConstants = 4,
		});

		// Create the root sig:
		globalRootSig->Finalize("BRM Global Root", D3D12_ROOT_SIGNATURE_FLAG_NONE);

		return globalRootSig;
	}


	inline uint8_t GetFrameOffsetIdx(uint64_t frameNum, uint8_t numFramesInFlight)
	{
		return frameNum % numFramesInFlight;
	}
}


namespace dx12
{
	void IBindlessResource::GetResourceUseState(void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");

		constexpr D3D12_RESOURCE_STATES k_defaultResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		SEAssert(dest && destByteSize == sizeof(D3D12_RESOURCE_STATES), "Invalid destination size");
		memcpy(dest, &k_defaultResourceState, destByteSize);
	}


	// ---


	void BindlessResourceManager::PlatObj::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			if (m_isCreated)
			{
				for (auto& cache : m_cpuDescriptorCache)
				{
					cache.clear();
				}

				m_resourceCache.clear();
				m_usageStateCache.clear();

				m_deviceCache = nullptr;

				m_nullDescriptor = { 0 };
				m_elementSize = 0;
				m_numActiveResources = 0;
				m_numFramesInFlight = 0;

				m_globalRootSig = nullptr;

				for (auto& heap : m_gpuDescriptorHeaps)
				{
					heap = nullptr;
				}

				// re::BindlessResourceManager::PlatObj:
				m_currentMaxIndex = re::BindlessResourceManager::k_initialResourceCount;
				m_isCreated = false;
			}
		}
	}


	void BindlessResourceManager::Initialize(re::BindlessResourceManager& brm, uint64_t frameNum)
	{
		dx12::BindlessResourceManager::PlatObj* brmPlatObj =
			brm.GetPlatformObject()->As<dx12::BindlessResourceManager::PlatObj*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatObj->m_platformParamsMutex);

			const uint32_t totalResourceIndexes = brmPlatObj->m_currentMaxIndex;

			if (brmPlatObj->m_isCreated == false) // First initialization: 
			{
				const uint8_t numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();

				brmPlatObj->m_cpuDescriptorCache.resize(numFramesInFlight);

				brmPlatObj->m_deviceCache = re::RenderManager::Get()->GetContext()->As<dx12::Context*>()->GetDevice().GetD3DDevice().Get();

				brmPlatObj->m_elementSize = brmPlatObj->m_deviceCache->GetDescriptorHandleIncrementSize(k_brmHeapType);
				SEAssert(brmPlatObj->m_elementSize > 0, "Invalid element size");

				// Create a null descriptor:
				// We don't actually have enough information to create a valid null descriptor (as we're overlaying many
				// resource types within the same root signature), so we just pick something reasonable as we'll never 
				// actually access ones of these unused descriptors
				brmPlatObj->m_nullDescriptor = re::RenderManager::Get()->GetContext()->As<dx12::Context*>()->GetNullSRVDescriptor(
					D3D12_SRV_DIMENSION_BUFFER,
					DXGI_FORMAT_R32G32B32A32_UINT).GetBaseDescriptor();

				brmPlatObj->m_numActiveResources = 0;

				brmPlatObj->m_numFramesInFlight = numFramesInFlight;				

				brmPlatObj->m_globalRootSig = CreateGlobalBRMRootSignature();

				brmPlatObj->m_isCreated = true;
			}

			// Deferred-delete any existing shader-visible descriptor heap via a temporary PlatObj:
			std::unique_ptr<dx12::BindlessResourceManager::PlatObj> paramsToDelete =
				std::make_unique<dx12::BindlessResourceManager::PlatObj>();

			paramsToDelete->m_gpuDescriptorHeaps = std::move(brmPlatObj->m_gpuDescriptorHeaps);
			re::RenderManager::Get()->RegisterForDeferredDelete(std::move(paramsToDelete));
			brmPlatObj->m_gpuDescriptorHeaps.resize(brmPlatObj->m_numFramesInFlight);

			// Initialize/grow our non-frame-indexed cache vectors (No-op if old size == new size)
			brmPlatObj->m_resourceCache.resize(totalResourceIndexes, nullptr);
			brmPlatObj->m_usageStateCache.resize(totalResourceIndexes, D3D12_RESOURCE_STATE_COMMON);

			// Create and initialize replacement heaps:
			for (uint8_t frameIdx = 0; frameIdx < brmPlatObj->m_numFramesInFlight; ++frameIdx)
			{
				// Initialize/grow the CPU-visible descriptor cache:
				brmPlatObj->m_cpuDescriptorCache[frameIdx].resize(
					totalResourceIndexes,
					brmPlatObj->m_nullDescriptor);

				// Initialize/grow the GPU-visible descriptor cache:
				brmPlatObj->m_gpuDescriptorHeaps[frameIdx] = CreateShaderVisibleDescriptorHeaps(
					brmPlatObj->m_deviceCache,
					totalResourceIndexes,
					frameIdx);

				// Copy descriptors into the new heap:
				const D3D12_CPU_DESCRIPTOR_HANDLE destCPUHandle =
					brmPlatObj->m_gpuDescriptorHeaps[frameIdx]->GetCPUDescriptorHandleForHeapStart();

				brmPlatObj->m_deviceCache->CopyDescriptors(
					1,													// UINT NumDestDescriptorRanges
					&destCPUHandle,										// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
					&totalResourceIndexes,								// const UINT* pDestDescriptorRangeSizes
					totalResourceIndexes,								// UINT NumSrcDescriptorRanges
					brmPlatObj->m_cpuDescriptorCache[frameIdx].data(),	// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
					nullptr,											// const UINT* pSrcDescriptorRangeSizes
					k_brmHeapType										// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType
				);
			}
		}
	}


	void BindlessResourceManager::SetResource(
		re::BindlessResourceManager& brm, re::IBindlessResource* resource, ResourceHandle index)
	{
		dx12::BindlessResourceManager::PlatObj* brmPlatObj =
			brm.GetPlatformObject()->As<dx12::BindlessResourceManager::PlatObj*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatObj->m_platformParamsMutex);

			SEAssert(brmPlatObj->m_isCreated, "BindlessResourceManager has not been created");
			SEAssert(index < brmPlatObj->m_resourceCache.size(), "Index is OOB");

			if (resource)
			{
				SEAssert(brmPlatObj->m_resourceCache[index] == nullptr &&
					brmPlatObj->m_usageStateCache[index] == D3D12_RESOURCE_STATE_COMMON,
					"A resource cache entry is not zero-initialized");

				// Add the resource pointer to the resource cache
				// Note: May be null if resource doesn't want to participate in resource transitions
				resource->GetPlatformResource(&brmPlatObj->m_resourceCache[index], sizeof(ID3D12Resource*));

				for (uint8_t frameOffsetIdx = 0; frameOffsetIdx < brmPlatObj->m_numFramesInFlight; ++frameOffsetIdx)
				{
					SEAssert(brmPlatObj->m_cpuDescriptorCache[frameOffsetIdx][index].ptr == brmPlatObj->m_nullDescriptor.ptr,
						"A resource cache entry is not zero-initialized");

					// Add the resource descriptor to the CPU-visible descriptor cache:
					resource->GetDescriptor(
						&brmPlatObj->m_cpuDescriptorCache[frameOffsetIdx][index],
						sizeof(D3D12_CPU_DESCRIPTOR_HANDLE),
						frameOffsetIdx);
					
					SEAssert(brmPlatObj->m_cpuDescriptorCache[frameOffsetIdx][index].ptr != 0,
						"Failed to get descriptor handle");
				}

				// Add the default resource usage state to the cache:
				resource->GetResourceUseState(&brmPlatObj->m_usageStateCache[index], sizeof(D3D12_RESOURCE_STATES));
				SEAssert(brmPlatObj->m_usageStateCache[index] != D3D12_RESOURCE_STATE_COMMON,
					"Failed to get descriptor handle");

				brmPlatObj->m_numActiveResources++;
				SEAssert(brmPlatObj->m_numActiveResources <= brmPlatObj->m_resourceCache.size(),
					"Number of active resources is out of bounds");
			}
			else // Otherwise, zero out the caches:
			{
				SEAssert(brmPlatObj->m_usageStateCache[index] != D3D12_RESOURCE_STATE_COMMON,
					"Trying to release a resource cache entry that is already zero-initialized");

				brmPlatObj->m_resourceCache[index] = nullptr;
				brmPlatObj->m_usageStateCache[index] = D3D12_RESOURCE_STATE_COMMON;

				for (auto& frameEntry : brmPlatObj->m_cpuDescriptorCache)
				{
					SEAssert(frameEntry[index].ptr != brmPlatObj->m_nullDescriptor.ptr,
						"Trying to release a resource cache entry that is already zero-initialized");

					frameEntry[index] = brmPlatObj->m_nullDescriptor;
				}

				SEAssert(brmPlatObj->m_numActiveResources > 0, "About to underflow m_numActiveResources");
				brmPlatObj->m_numActiveResources--;
			}

			// Finally, copy the descriptor into our GPU-visible heaps. This is safe for all N buffers, as we're either
			// inserting into an empty location, or replacing a descriptor that was released N frames ago
			const uint32_t destOffset = (index * brmPlatObj->m_elementSize);
			for (uint8_t frameOffsetIdx = 0; frameOffsetIdx < brmPlatObj->m_numFramesInFlight; ++frameOffsetIdx)
			{
				const D3D12_CPU_DESCRIPTOR_HANDLE destCPUHandle(
					brmPlatObj->m_gpuDescriptorHeaps[frameOffsetIdx]->GetCPUDescriptorHandleForHeapStart().ptr + destOffset);

				brmPlatObj->m_deviceCache->CopyDescriptorsSimple(
					1,
					destCPUHandle,
					brmPlatObj->m_cpuDescriptorCache[frameOffsetIdx][index],
					k_brmHeapType);
			}
		}
	}


	std::vector<dx12::CommandList::TransitionMetadata> BindlessResourceManager::BuildResourceTransitions(
		re::BindlessResourceManager const& brm)
	{
		dx12::BindlessResourceManager::PlatObj* brmPlatObj =
			brm.GetPlatformObject()->As<dx12::BindlessResourceManager::PlatObj*>();
		SEAssert(brmPlatObj->m_isCreated, "BindlessResourceManager has not been created");

		{
			std::lock_guard<std::mutex> lock(brmPlatObj->m_platformParamsMutex);

			// Batch all transitions for all resources into a single call:
			std::vector<dx12::CommandList::TransitionMetadata> transitions;
			transitions.reserve(static_cast<uint64_t>(brmPlatObj->m_currentMaxIndex));

			uint32_t numSeenResources = 0;
			for (uint32_t i = 0; i < brmPlatObj->m_resourceCache.size(); ++i)
			{
				if (brmPlatObj->m_resourceCache[i])
				{
					transitions.emplace_back(dx12::CommandList::TransitionMetadata{
						.m_resource = brmPlatObj->m_resourceCache[i],
						.m_toState = brmPlatObj->m_usageStateCache[i],
						.m_subresourceIndexes = {D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES},
						});

					numSeenResources++;

					// Early out if we've found the same number of valid resource pointers as active resources
					if (numSeenResources == brmPlatObj->m_numActiveResources)
					{
						break;
					}
				}
			}

			return transitions;
		}
	}


	dx12::RootSignature const* BindlessResourceManager::GetRootSignature(re::BindlessResourceManager const& brm)
	{
		dx12::BindlessResourceManager::PlatObj* brmPlatObj =
			brm.GetPlatformObject()->As<dx12::BindlessResourceManager::PlatObj*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatObj->m_platformParamsMutex);
			
			SEAssert(brmPlatObj->m_isCreated, "BindlessResourceManager has not been created");

			return brmPlatObj->m_globalRootSig.get();
		}
	}


	ID3D12DescriptorHeap* BindlessResourceManager::GetDescriptorHeap(
		re::BindlessResourceManager const& brm, uint64_t frameNum)
	{
		dx12::BindlessResourceManager::PlatObj* brmPlatObj =
			brm.GetPlatformObject()->As<dx12::BindlessResourceManager::PlatObj*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatObj->m_platformParamsMutex);

			SEAssert(brmPlatObj->m_isCreated, "BindlessResourceManager has not been created");

			const uint8_t frameOffsetIdx = GetFrameOffsetIdx(frameNum, brmPlatObj->m_numFramesInFlight);

			return brmPlatObj->m_gpuDescriptorHeaps[frameOffsetIdx].Get();
		}
	}
}