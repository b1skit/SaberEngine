// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager_DX12.h"
#include "Context_DX12.h"
#include "SysInfo_DX12.h"


namespace
{
	D3D12_CPU_DESCRIPTOR_HANDLE GetNullDescriptor(re::IBindlessResourceSet const& resourceSet)
	{
		re::BindlessResourceManager* brm = resourceSet.GetBindlessResourceManager();

		dx12::BindlessResourceManager::PlatformParams* platParams =
			brm->GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		SEAssert(platParams->m_isCreated == true, "BindlessResourceManager PlatformParams have not been created");
		SEAssert(platParams->m_rootSignature, "No root signature has been set");

		dx12::RootSignature::RootParameter const* rootParam =
			platParams->m_rootSignature->GetRootSignatureEntry(resourceSet.GetShaderName());
		SEAssert(rootParam->m_type == dx12::RootSignature::RootParameter::Type::DescriptorTable,
			"Unexpected root parameter type");

		// We need to know what type of null descriptor to set: Find the root signature range entry we're initializing:
		dx12::RootSignature::RangeEntry const* rangeEntryPtr = nullptr;
		dx12::RootSignature::DescriptorType descriptorType = dx12::RootSignature::DescriptorType::Type_Invalid;

		std::vector<dx12::RootSignature::DescriptorTable> const& descriptorTableMetadata =
			platParams->m_rootSignature->GetDescriptorTableMetadata();
		for (auto const& descriptorTable : descriptorTableMetadata)
		{
			if (descriptorTable.m_index == rootParam->m_index)
			{
				for (uint8_t rangeTypeIdx = 0; rangeTypeIdx < dx12::RootSignature::DescriptorType::Type_Count; ++rangeTypeIdx)
				{
					std::vector<dx12::RootSignature::RangeEntry> const& ranges = descriptorTable.m_ranges[rangeTypeIdx];
					SEAssert(ranges.size() <= 1, "Only expecting a single range of a single type for bindless resources");

					for (auto const& rangeEntry : ranges)
					{
						rangeEntryPtr = &rangeEntry;
						descriptorType = static_cast<dx12::RootSignature::DescriptorType>(rangeTypeIdx);
						break;
					}

					if (rangeEntryPtr)
					{
						break;
					}
				}
			}

			if (rangeEntryPtr)
			{
				break;
			}
		}
		SEAssert(rangeEntryPtr, "Failed to find descriptor in table ranges");

		// Get the null descriptor:
		D3D12_CPU_DESCRIPTOR_HANDLE result{};

		dx12::IBindlessResourceSet::PlatformParams* resourceSetPlatParams =
			resourceSet.GetPlatformParams()->As<dx12::IBindlessResourceSet::PlatformParams*>();
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		switch (descriptorType)
		{
		case dx12::RootSignature::DescriptorType::SRV:
		{
			result = context->GetNullSRVDescriptor(
				rangeEntryPtr->m_srvDesc.m_viewDimension,
				rangeEntryPtr->m_srvDesc.m_format).GetBaseDescriptor();
		}
		break;
		case dx12::RootSignature::DescriptorType::UAV:
		{
			result = context->GetNullUAVDescriptor(
				rangeEntryPtr->m_uavDesc.m_viewDimension,
				rangeEntryPtr->m_uavDesc.m_format).GetBaseDescriptor();
		}
		break;
		case dx12::RootSignature::DescriptorType::CBV:
		{
			result = context->GetNullCBVDescriptor().GetBaseDescriptor();
		}
		break;
		default: SEAssertF("Invalid descriptor type");
		}

		return result;
	}
}

namespace dx12
{
	void IBindlessResourceSet::PlatformParams::Destroy()
	{
		m_isCreated = false;
	}


	void IBindlessResourceSet::Initialize(re::IBindlessResourceSet& resourceSet)
	{
		dx12::IBindlessResourceSet::PlatformParams* resourceSetPlatParams =
			resourceSet.GetPlatformParams()->As<dx12::IBindlessResourceSet::PlatformParams*>();
		SEAssert(resourceSetPlatParams, "Resource set platform parameters are null");

		// First initialization: Create the CPU-side descriptor cache and null-initialize it
		if (resourceSetPlatParams->m_isCreated == false)
		{
			resourceSetPlatParams->m_deviceCache = 
				re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDevice().Get();

			// Initialize the descriptor cache will null descriptors:
			D3D12_CPU_DESCRIPTOR_HANDLE const& nullDescriptor = GetNullDescriptor(resourceSet);

			resourceSetPlatParams->m_cpuDescriptorCache.resize(resourceSet.GetMaxResourceCount(), nullDescriptor);

			resourceSetPlatParams->m_resourceCache.resize(resourceSet.GetMaxResourceCount(), nullptr);

			resourceSetPlatParams->m_numActiveResources = 0;

			resourceSetPlatParams->m_isCreated = true;
		}
		
		// Copy the descriptor cache to the BRM's descriptor heap:
		re::BindlessResourceManager const* brm = resourceSet.GetBindlessResourceManager();
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			brm->GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();

		const D3D12_CPU_DESCRIPTOR_HANDLE relativeDestBase{
			.ptr = brmPlatParams->m_gpuCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr +
				resourceSet.GetBaseOffset() * brmPlatParams->m_elementSize,
		};
		
		const uint32_t numSrcDescriptors = resourceSet.GetMaxResourceCount();
		
		// Note: Our source descriptors are not contiguous, but our destination descriptors are
		resourceSetPlatParams->m_deviceCache->CopyDescriptors(
			1,														// UINT NumDestDescriptorRanges
			&relativeDestBase,										// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
			&numSrcDescriptors,										// const UINT* pDestDescriptorRangeSizes
			numSrcDescriptors,										// UINT NumSrcDescriptorRanges
			resourceSetPlatParams->m_cpuDescriptorCache.data(),		// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
			nullptr,												// const UINT* pSrcDescriptorRangeSizes
			BindlessResourceManager::PlatformParams::k_brmHeapType	// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType
		);
	}


	void IBindlessResourceSet::SetResource(
		re::IBindlessResourceSet& resourceSet, re::IBindlessResource* resource, ResourceHandle index)
	{
		dx12::IBindlessResourceSet::PlatformParams* resourceSetPlatParams =
			resourceSet.GetPlatformParams()->As<dx12::IBindlessResourceSet::PlatformParams*>();
		SEAssert(resourceSetPlatParams->m_isCreated, "Resource set has not been created");
		SEAssert(index < resourceSetPlatParams->m_cpuDescriptorCache.size(), "Index is OOB");

		if (resource)
		{
			// Add the resource descriptor to the CPU-visible descriptor cache:
			resource->GetDescriptor(
				&resourceSet,
				&resourceSetPlatParams->m_cpuDescriptorCache[index],
				sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
			SEAssert(resourceSetPlatParams->m_cpuDescriptorCache[index].ptr != 0, "Failed to get descriptor handle");

			// Add the resource pointer to the resource cache:
			ID3D12Resource* resourcePtr = nullptr;
			resource->GetPlatformResource(&resourcePtr, sizeof(ID3D12Resource*));
			SEAssert(resourcePtr != nullptr, "Failed to get a valid D3D resource");

			resourceSetPlatParams->m_resourceCache[index] = resourcePtr;

			resourceSetPlatParams->m_numActiveResources++;
		}
		else // Otherwise, write a null resource and descriptor:
		{
			resourceSetPlatParams->m_cpuDescriptorCache[index] = GetNullDescriptor(resourceSet);
			resourceSetPlatParams->m_resourceCache[index] = nullptr;

			SEAssert(resourceSetPlatParams->m_numActiveResources > 0, "About to underflow m_numActiveResources");
			resourceSetPlatParams->m_numActiveResources--;
		}

		// Set the CPU-visible descriptor in the BRM's gpu-visible descriptor heap:
		// Get the *relative* base handle for the GPU-visible heap:
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			resourceSet.GetBindlessResourceManager()->GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		
		const D3D12_CPU_DESCRIPTOR_HANDLE destHandle{
			.ptr = brmPlatParams->m_gpuCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr +
				(resourceSet.GetBaseOffset() * brmPlatParams->m_elementSize) +
					(index * brmPlatParams->m_elementSize),
		};

		resourceSetPlatParams->m_deviceCache->CopyDescriptorsSimple(
			1,															// NumDescriptors
			destHandle,													// DestDescriptorRangeStart
			resourceSetPlatParams->m_cpuDescriptorCache[index],			// SrcDescriptorRangeStart
			BindlessResourceManager::PlatformParams::k_brmHeapType);	// DescriptorHeapsType
	}


	// ---


	void BindlessResourceManager::PlatformParams::Destroy()
	{
		m_rootSignature = nullptr;
		m_gpuCbvSrvUavDescriptorHeap = nullptr;
		m_isCreated = false;
	}


	void BindlessResourceManager::Create(re::BindlessResourceManager& brm, uint32_t totalDescriptors)
	{
		SEAssert(totalDescriptors > 0, "Invalid number of descriptors");

		dx12::BindlessResourceManager::PlatformParams* platParams = 
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();

		platParams->m_deviceCache = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDevice().Get();

		// Create our GPU-visible descriptor heap:
		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{
			.Type = BindlessResourceManager::PlatformParams::k_brmHeapType,
			.NumDescriptors = totalDescriptors,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask()
		};

		CheckHResult(
			platParams->m_deviceCache->CreateDescriptorHeap(
				&descriptorHeapDesc, IID_PPV_ARGS(&platParams->m_gpuCbvSrvUavDescriptorHeap)),
			"Failed to create descriptor heap");

		platParams->m_gpuCbvSrvUavDescriptorHeap->SetName(L"Bindless Resource Manager GPU-visible heap");

		platParams->m_elementSize = platParams->m_deviceCache->GetDescriptorHandleIncrementSize(
			BindlessResourceManager::PlatformParams::k_brmHeapType);
		SEAssert(platParams->m_elementSize > 0, "Invalid element size");

		// Create the root signature:
		platParams->m_rootSignature = dx12::RootSignature::CreateUninitialized();

		// Have each resource set populate a DescriptorRangeCreateDesc: 
		std::vector<std::unique_ptr<re::IBindlessResourceSet>> const& resourceSets = brm.GetResourceSets();
	
		for (uint8_t resourceSetIdx = 0; resourceSetIdx < resourceSets.size(); ++resourceSetIdx)
		{
			// Add a single table with a single range per resource set
			dx12::RootSignature::DescriptorRangeCreateDesc resourceSetDescriptorRangeDesc{};
			resourceSets[resourceSetIdx]->PopulateRootSignatureDesc(&resourceSetDescriptorRangeDesc);

			platParams->m_rootSignature->AddDescriptorTable({ resourceSetDescriptorRangeDesc }, D3D12_SHADER_VISIBILITY_ALL);
		}
		

		constexpr D3D12_ROOT_SIGNATURE_FLAGS k_bindlessRootSigFlags = 
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
			D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

		platParams->m_rootSignature->Finalize("BindlessResourceManager root signature", k_bindlessRootSigFlags);
	}


	ID3D12RootSignature* BindlessResourceManager::GetRootSignature(
		re::BindlessResourceManager const& brm)
	{
		dx12::BindlessResourceManager::PlatformParams* platParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		SEAssert(platParams->m_isCreated == true, "BindlessResourceManager PlatformParams have not been created");

		SEAssert(platParams->m_rootSignature, "No root signature has been set");

		return platParams->m_rootSignature->GetD3DRootSignature();
	}


	ID3D12DescriptorHeap* BindlessResourceManager::GetDescriptorHeap(
		re::BindlessResourceManager const& brm)
	{
		dx12::BindlessResourceManager::PlatformParams* platParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();
		SEAssert(platParams->m_isCreated == true, "BindlessResourceManager PlatformParams have not been created");

		return platParams->m_gpuCbvSrvUavDescriptorHeap.Get();
	}


	std::vector<dx12::CommandList::TransitionMetadata> BindlessResourceManager::BuildResourceTransitions(
		re::BindlessResourceManager const& brm)
	{
		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			brm.GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();

		// Batch all transitions for all resources into a single call:
		std::vector<dx12::CommandList::TransitionMetadata> transitions;
		transitions.reserve(
			static_cast<uint64_t>(brm.GetNumResourceSets()) * re::BindlessResourceManager::k_maxResourceCount);

		for (auto const& resourceSet : brm.GetResourceSets())
		{
			dx12::RootSignature::RootParameter const* tableRootParam =
				brmPlatParams->m_rootSignature->GetRootSignatureEntry(resourceSet->GetShaderName());
			SEAssert(tableRootParam->m_type == dx12::RootSignature::RootParameter::Type::DescriptorTable,
				"Unexpected root parameter type");

			// Infer the destination resource state from the descriptor table type
			D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_COMMON;
			switch (tableRootParam->m_tableEntry.m_type)
			{
			case dx12::RootSignature::DescriptorType::SRV:
			{
				toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}
			break;
			case dx12::RootSignature::DescriptorType::UAV:
			{
				toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}
			break;
			case dx12::RootSignature::DescriptorType::CBV:
			{
				toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}
			break;
			default: SEAssertF("Invalid descriptor type");
			}

			dx12::IBindlessResourceSet::PlatformParams* resourceSetPlatParams =
				resourceSet->GetPlatformParams()->As<dx12::IBindlessResourceSet::PlatformParams*>();

			uint32_t numSeenResources = 0;
			for (auto const& resourcePtr : resourceSetPlatParams->m_resourceCache)
			{
				if (resourcePtr)
				{
					transitions.emplace_back(dx12::CommandList::TransitionMetadata{
						.m_resource = resourcePtr,
						.m_toState = toState,
						.m_subresourceIndexes = {D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES},
						});

					numSeenResources++;
				}

				// Early out if we've found the same number of valid resource pointers as active resources
				if (numSeenResources == resourceSetPlatParams->m_numActiveResources)
				{
					break;
				}
			}
		}

		return transitions;
	}
}