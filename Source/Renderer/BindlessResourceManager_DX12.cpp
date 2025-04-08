// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager.h"
#include "BindlessResourceManager_DX12.h"
#include "Context_DX12.h"
#include "RenderManager.h"
#include "RootSignature_DX12.h"
#include "SysInfo_DX12.h"

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
		constexpr uint32_t k_firstBindlessSpaceIdx = 20;
		uint32_t bindlessResourceSpace = k_firstBindlessSpaceIdx;

		// Bindless LUT:
		//--------------
		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "BindlessLUT",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = bindlessResourceSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});


		// Buffers:
		//---------
		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "CameraParams",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = bindlessResourceSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});


		// Textures:
		//----------
		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "Texture_RW2D",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = bindlessResourceSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_uavDesc = {
				.m_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.m_viewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
			}
		});


		// Vertex streams:
		//----------------
		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "VertexStreams_UShort",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = bindlessResourceSpace++,
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
				.RegisterSpace = bindlessResourceSpace++,
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
				.RegisterSpace = bindlessResourceSpace++,
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
				.RegisterSpace = bindlessResourceSpace++,
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
				.RegisterSpace = bindlessResourceSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});


		// For now, we only use bindless resources in DXR, so we hard-code the root signature to match.
		// TODO: Generalize the root signature creation (or define it directly in HLSL) so we can use bindless resources
		// in any/all shaders
		constexpr uint32_t k_firstReservedSpaceIdx = 0;
		uint32_t reservedRegisterSpace = k_firstReservedSpaceIdx;

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "SceneBVH",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = reservedRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
			}
		});

		tableRanges.emplace_back(dx12::RootSignature::DescriptorRangeCreateDesc{
			.m_shaderName = "TraceRayParams",
			.m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
				.NumDescriptors = std::numeric_limits<uint32_t>::max(), // Unbounded
				.BaseShaderRegister = 0,
				.RegisterSpace = reservedRegisterSpace++,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
				.OffsetInDescriptorsFromTableStart = 0,
			},
			.m_srvDesc = {
				.m_format = DXGI_FORMAT_UNKNOWN,
				.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER,
			}
		});


		// Add our overlapping ranges as a single descriptor table:
		globalRootSig->AddDescriptorTable(tableRanges, D3D12_SHADER_VISIBILITY_ALL);

		// Root constant:
		globalRootSig->AddRootParameter(dx12::RootSignature::RootParameterCreateDesc{
			.m_shaderName = "GlobalConstants",
			.m_type = dx12::RootSignature::RootParameter::Type::Constant,
			.m_registerBindPoint = 0,
			.m_registerSpace = reservedRegisterSpace++,
			.m_flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
			.m_visibility = D3D12_SHADER_VISIBILITY_ALL,
			.m_numRootConstants = 4,
			});

		// Create the root sig:
		globalRootSig->Finalize("BRM Global Root", D3D12_ROOT_SIGNATURE_FLAG_NONE);

		return globalRootSig;
	}


	inline uint8_t GetFrameIndex(uint64_t frameNum, uint8_t numFramesInFlight)
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


	void BindlessResourceManager::PlatformParams::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			if (m_isCreated)
			{
				m_resourceCache.clear();
				m_cpuDescriptorCache.clear();
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

				// re::BindlessResourceManager::PlatformParams:
				m_currentMaxIndex = re::BindlessResourceManager::k_initialResourceCount;
				m_isCreated = false;
			}
		}
	}


	void BindlessResourceManager::Initialize(re::BindlessResourceManager& brm, uint64_t frameNum)
	{
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatParams->m_platformParamsMutex);

			const uint32_t totalResourceIndexes = brmPlatParams->m_currentMaxIndex;

			if (brmPlatParams->m_isCreated == false) // First initialization: 
			{
				brmPlatParams->m_deviceCache = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDevice().Get();

				brmPlatParams->m_elementSize = 
					brmPlatParams->m_deviceCache->GetDescriptorHandleIncrementSize(k_brmHeapType);
				SEAssert(brmPlatParams->m_elementSize > 0, "Invalid element size");

				// Create a null descriptor:
				// We don't actually have enough information to create a valid null descriptor (as we're overlaying many
				// resource types within the same root signature), so we just pick something reasonable as we'll never 
				// actually access ones of these unused descriptors
				brmPlatParams->m_nullDescriptor = re::Context::GetAs<dx12::Context*>()->GetNullSRVDescriptor(
					D3D12_SRV_DIMENSION_BUFFER,
					DXGI_FORMAT_R32G32B32A32_UINT).GetBaseDescriptor();

				brmPlatParams->m_numActiveResources = 0;

				brmPlatParams->m_numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();

				brmPlatParams->m_globalRootSig = CreateGlobalBRMRootSignature();

				brmPlatParams->m_isCreated = true;
			}

			// Initialize/grow our CPU-visible cache vectors (No-op if old size == new size)
			brmPlatParams->m_resourceCache.resize(totalResourceIndexes, nullptr);
			brmPlatParams->m_cpuDescriptorCache.resize(totalResourceIndexes, brmPlatParams->m_nullDescriptor);
			brmPlatParams->m_usageStateCache.resize(totalResourceIndexes, D3D12_RESOURCE_STATE_COMMON);

			// Deferred-delete any existing shader-visible descriptor heap via a temporary PlatformParams:
			std::unique_ptr<dx12::BindlessResourceManager::PlatformParams> paramsToDelete =
				std::make_unique<dx12::BindlessResourceManager::PlatformParams>();

			paramsToDelete->m_gpuDescriptorHeaps = std::move(brmPlatParams->m_gpuDescriptorHeaps);
			re::RenderManager::Get()->RegisterForDeferredDelete(std::move(paramsToDelete));


			// Create and initialize replacement heaps:
			for (uint8_t i = 0; i < brmPlatParams->m_numFramesInFlight; ++i)
			{
				brmPlatParams->m_gpuDescriptorHeaps[i] = CreateShaderVisibleDescriptorHeaps(
					brmPlatParams->m_deviceCache,
					totalResourceIndexes,
					i);

				// Copy descriptors into the new heap:
				const D3D12_CPU_DESCRIPTOR_HANDLE destHandle =
					brmPlatParams->m_gpuDescriptorHeaps[i]->GetCPUDescriptorHandleForHeapStart();

				brmPlatParams->m_deviceCache->CopyDescriptors(
					1,											// UINT NumDestDescriptorRanges
					&destHandle,								// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
					&totalResourceIndexes,						// const UINT* pDestDescriptorRangeSizes
					totalResourceIndexes,						// UINT NumSrcDescriptorRanges
					brmPlatParams->m_cpuDescriptorCache.data(),	// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
					nullptr,									// const UINT* pSrcDescriptorRangeSizes
					k_brmHeapType								// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType
				);
			}
		}
	}


	void BindlessResourceManager::SetResource(
		re::BindlessResourceManager& brm, re::IBindlessResource* resource, ResourceHandle index)
	{
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatParams->m_platformParamsMutex);

			SEAssert(brmPlatParams->m_isCreated, "BindlessResourceManager has not been created");
			SEAssert(index < brmPlatParams->m_resourceCache.size(), "Index is OOB");

			if (resource)
			{
				SEAssert(brmPlatParams->m_resourceCache[index] == nullptr &&
					brmPlatParams->m_cpuDescriptorCache[index].ptr == brmPlatParams->m_nullDescriptor.ptr &&
					brmPlatParams->m_usageStateCache[index] == D3D12_RESOURCE_STATE_COMMON,
					"A resource cache entry is not zero-initialized");

				// Add the resource pointer to the resource cache
				// Note: May be null if resource doesn't want to participate in resource transitions
				resource->GetPlatformResource(&brmPlatParams->m_resourceCache[index], sizeof(ID3D12Resource*));

				// Add the resource descriptor to the CPU-visible descriptor cache:
				resource->GetDescriptor(
					&brmPlatParams->m_cpuDescriptorCache[index],
					sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
				SEAssert(brmPlatParams->m_cpuDescriptorCache[index].ptr != 0, "Failed to get descriptor handle");

				// Add the default resource usage state to the cache:
				resource->GetResourceUseState(&brmPlatParams->m_usageStateCache[index], sizeof(D3D12_RESOURCE_STATES));
				SEAssert(brmPlatParams->m_usageStateCache[index] != D3D12_RESOURCE_STATE_COMMON,
					"Failed to get descriptor handle");

				brmPlatParams->m_numActiveResources++;
				SEAssert(brmPlatParams->m_numActiveResources <= brmPlatParams->m_resourceCache.size(),
					"Number of active resources is out of bounds");
			}
			else // Otherwise, zero out the caches:
			{
				SEAssert(brmPlatParams->m_cpuDescriptorCache[index].ptr != brmPlatParams->m_nullDescriptor.ptr &&
					brmPlatParams->m_usageStateCache[index] != D3D12_RESOURCE_STATE_COMMON,
					"Trying to release a resource cache entry that is already zero-initialized");

				brmPlatParams->m_resourceCache[index] = nullptr;
				brmPlatParams->m_cpuDescriptorCache[index] = brmPlatParams->m_nullDescriptor;
				brmPlatParams->m_usageStateCache[index] = D3D12_RESOURCE_STATE_COMMON;

				SEAssert(brmPlatParams->m_numActiveResources > 0, "About to underflow m_numActiveResources");
				brmPlatParams->m_numActiveResources--;
			}

			// Finally, copy the descriptor into our GPU-visible heaps. This is safe for all N buffers, as we're either
			// inserting into an empty location, or replacing a descriptor that was released N frames ago
			for (uint8_t i = 0; i < brmPlatParams->m_numFramesInFlight; ++i)
			{
				const D3D12_CPU_DESCRIPTOR_HANDLE destHandle(
					brmPlatParams->m_gpuDescriptorHeaps[i]->GetCPUDescriptorHandleForHeapStart().ptr +
					(index * brmPlatParams->m_elementSize));

				brmPlatParams->m_deviceCache->CopyDescriptorsSimple(
					1,
					destHandle,
					brmPlatParams->m_cpuDescriptorCache[index],
					k_brmHeapType);
			}

			// TODO: Handle cases where we use **DIFFERENT** parts of the same resource, depending on the frame
			// -> e.g. mutable buffers
		}
	}


	std::vector<dx12::CommandList::TransitionMetadata> BindlessResourceManager::BuildResourceTransitions(
		re::BindlessResourceManager const& brm)
	{
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		SEAssert(brmPlatParams->m_isCreated, "BindlessResourceManager has not been created");

		{
			std::lock_guard<std::mutex> lock(brmPlatParams->m_platformParamsMutex);

			// Batch all transitions for all resources into a single call:
			std::vector<dx12::CommandList::TransitionMetadata> transitions;
			transitions.reserve(static_cast<uint64_t>(brmPlatParams->m_currentMaxIndex));

			uint32_t numSeenResources = 0;
			for (uint32_t i = 0; i < brmPlatParams->m_resourceCache.size(); ++i)
			{
				if (brmPlatParams->m_resourceCache[i])
				{
					transitions.emplace_back(dx12::CommandList::TransitionMetadata{
						.m_resource = brmPlatParams->m_resourceCache[i],
						.m_toState = brmPlatParams->m_usageStateCache[i],
						.m_subresourceIndexes = {D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES},
						});

					numSeenResources++;

					// Early out if we've found the same number of valid resource pointers as active resources
					if (numSeenResources == brmPlatParams->m_numActiveResources)
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
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatParams->m_platformParamsMutex);
			
			SEAssert(brmPlatParams->m_isCreated, "BindlessResourceManager has not been created");

			return brmPlatParams->m_globalRootSig.get();
		}
	}


	ID3D12DescriptorHeap* BindlessResourceManager::GetDescriptorHeap(
		re::BindlessResourceManager const& brm, uint64_t frameNum)
	{
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		{
			std::lock_guard<std::mutex> lock(brmPlatParams->m_platformParamsMutex);

			SEAssert(brmPlatParams->m_isCreated, "BindlessResourceManager has not been created");

			const uint8_t frameIndex = GetFrameIndex(frameNum, brmPlatParams->m_numFramesInFlight);

			return brmPlatParams->m_gpuDescriptorHeaps[frameIndex].Get();
		}
	}
}