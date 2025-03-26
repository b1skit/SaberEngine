// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager.h"
#include "BindlessResourceManager_DX12.h"
#include "Context_DX12.h"

#include "Core/Util/CastUtils.h"

#include "Shaders/Common/BindlessResourceParams.h"

#include <d3d12.h>
#include <wrl/client.h>


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

			// Get a null descriptor:
			resourceSet.GetNullDescriptor(&resourceSetPlatParams->m_nullDescriptor, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
			SEAssert(resourceSetPlatParams->m_nullDescriptor.ptr != 0, "Failed to get a valid null descriptor");

			// Get the default usage state:
			resourceSetPlatParams->m_usageState = D3D12_RESOURCE_STATE_COMMON;

			resourceSet.GetResourceUsageState(&resourceSetPlatParams->m_usageState, sizeof(D3D12_RESOURCE_STATES));
			SEAssert(resourceSetPlatParams->m_usageState != D3D12_RESOURCE_STATE_COMMON,
				"Resource state is common. This is unexpected");

			// Initialize the the descriptor cache with our null descriptor:
			resourceSetPlatParams->m_cpuDescriptorCache.resize(
				resourceSet.GetMaxResourceCount(),
				resourceSetPlatParams->m_nullDescriptor);

			resourceSetPlatParams->m_resourceCache.resize(resourceSet.GetMaxResourceCount(), nullptr);
			resourceSetPlatParams->m_numActiveResources = 0;
			resourceSetPlatParams->m_isCreated = true;
		}
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
			resourceSetPlatParams->m_cpuDescriptorCache[index] = resourceSetPlatParams->m_nullDescriptor;
			resourceSetPlatParams->m_resourceCache[index] = nullptr;

			SEAssert(resourceSetPlatParams->m_numActiveResources > 0, "About to underflow m_numActiveResources");
			resourceSetPlatParams->m_numActiveResources--;
		}
	}


	// ---


	std::vector<dx12::CommandList::TransitionMetadata> BindlessResourceManager::BuildResourceTransitions(
		re::BindlessResourceManager const& brm)
	{
		// Batch all transitions for all resources into a single call:
		std::vector<dx12::CommandList::TransitionMetadata> transitions;
		transitions.reserve(
			static_cast<uint64_t>(brm.GetNumResourceSets()) * re::BindlessResourceManager::k_maxResourceCount);

		for (auto const& resourceSet : brm.GetResourceSets())
		{
			dx12::IBindlessResourceSet::PlatformParams* resourceSetPlatParams =
				resourceSet->GetPlatformParams()->As<dx12::IBindlessResourceSet::PlatformParams*>();		

			uint32_t numSeenResources = 0;
			for (auto const& resourcePtr : resourceSetPlatParams->m_resourceCache)
			{
				if (resourcePtr)
				{
					transitions.emplace_back(dx12::CommandList::TransitionMetadata{
						.m_resource = resourcePtr,
						.m_toState = resourceSetPlatParams->m_usageState,
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