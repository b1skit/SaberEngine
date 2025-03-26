// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager_Platform.h"
#include "BindlessResourceManager.h"
#include "RenderManager.h"

#include "Core/Assert.h"
#include "Core/Logger.h"


namespace re
{
	IBindlessResourceSet::IBindlessResourceSet(
		BindlessResourceManager* brm,
		char const* shaderName,
		uint32_t numResources)
		: m_bindlessResourceMgr(brm)
		, m_platformParams(platform::IBindlessResourceSet::CreatePlatformParams())
		, m_shaderName(shaderName)
		, m_maxResources(numResources)
		, m_threadProtector(false)
		, m_numFramesInFlight(re::RenderManager::Get()->GetNumFramesInFlight())
		
	{
		SEAssert(m_bindlessResourceMgr && m_maxResources > 0, "Invalid resource set parameters");

		// Initialize the free index queue:
		uint32_t curIdx = 0;
		while (curIdx < m_maxResources)
		{
			m_freeIndexes.emplace(curIdx++);
		}
	}


	void IBindlessResourceSet::Destroy()
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		ProcessUnregistrations(std::numeric_limits<uint64_t>::max()); // Immediately unregister everything

		SEAssert(m_freeIndexes.size() == m_maxResources,
			"Some resource handles have not been returned to the bindless resource set");

		m_freeIndexes = {};

		re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platformParams));
	}


	ResourceHandle IBindlessResourceSet::RegisterResource(std::unique_ptr<IBindlessResource>&& newBindlessResource)
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		SEAssert(!m_freeIndexes.empty(), "No more free indexes. Consider increasing k_maxResourceCount");

		const ResourceHandle resourceIdx = m_freeIndexes.front();
		m_freeIndexes.pop();

		m_registrations.emplace_back(RegistrationMetadata{
			.m_resource = std::move(newBindlessResource),
			.m_resourceHandle = resourceIdx
			});

		return resourceIdx;
	}


	void IBindlessResourceSet::UnregisterResource(ResourceHandle& resourceIdx, uint64_t frameNum)
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		m_unregistrations.emplace(UnregistrationMetadata{ frameNum , resourceIdx });

		resourceIdx = k_invalidResourceHandle;
	}


	void IBindlessResourceSet::Update(uint64_t frameNum)
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		ProcessUnregistrations(frameNum);

		ProcessRegistrations();
	}


	void IBindlessResourceSet::ProcessRegistrations()
	{
		// Set the descriptors for any newly registered resources:
		for (auto& registration : m_registrations)
		{
			SetResource(registration.m_resource.get(), registration.m_resourceHandle); // Write the descriptor
		}
		m_registrations.clear();
	}


	void IBindlessResourceSet::ProcessUnregistrations(uint64_t frameNum)
	{
		m_threadProtector.ValidateThreadAccess();

		// Release freed resources after N frames have passed:
		while (!m_unregistrations.empty() &&
			m_unregistrations.front().m_unregistrationFrameNum + m_numFramesInFlight < frameNum)
		{
			// Set a null descriptor:
			platform::IBindlessResourceSet::SetResource(*this, nullptr, m_unregistrations.front().m_resourceHandle);

			// Resource sets immediate free resources
			m_freeIndexes.emplace(m_unregistrations.front().m_resourceHandle);

			m_unregistrations.pop();
		}
	}


	void IBindlessResourceSet::SetResource(IBindlessResource* resource, ResourceHandle resourceHandle)
	{
		platform::IBindlessResourceSet::SetResource(*this, resource, resourceHandle);
	}


	// ---


	BindlessResourceManager::BindlessResourceManager()
		: m_mustRecreate(true)
		, m_numFramesInFlight(re::RenderManager::Get()->GetNumFramesInFlight())
		, m_threadProtector(false)
	{
	}


	void BindlessResourceManager::Initialize()
	{
		LOG("Initializing BindlessResourceManager to manage %llu IBindlessResourceSets with a max %d resources each",
			m_resourceSets.size(), k_maxResourceCount);

		m_threadProtector.ValidateThreadAccess();

		for (auto& resourceSet : m_resourceSets)
		{
			// Initialze resource sets:
			platform::IBindlessResourceSet::Initialize(*resourceSet);
		}

		m_mustRecreate = false;
	}


	void BindlessResourceManager::Update(uint64_t frameNum)
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		if (m_mustRecreate)
		{
			Initialize();
		}

		// Update the resource sets:
		for (auto& resourceSet : m_resourceSets)
		{
			resourceSet->Update(frameNum);
		}
	}


	void BindlessResourceManager::Destroy()
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		for (auto& resourceSet : m_resourceSets)
		{
			resourceSet->Destroy();
		}
	}
}