// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager_Platform.h"
#include "BindlessResourceManager.h"
#include "RenderManager.h"

#include "Core/Assert.h"
#include "Core/Logger.h"


namespace re
{
	void IBindlessResource::GetResourceUseState(void* dest, size_t destByteSize) const
	{
		platform::IBindlessResource::GetResourceUseState(dest, destByteSize);
	}


	// ---


	BindlessResourceManager::BindlessResourceManager()
		: m_platObj(platform::BindlessResourceManager::CreatePlatformObject())
		, m_mustReinitialize(true)
		, m_numFramesInFlight(re::RenderManager::Get()->GetNumFramesInFlight())
	{
		// Initialize the free index queue:
		uint32_t curIdx = 0;
		while (curIdx < m_platObj->m_currentMaxIndex)
		{
			m_freeIndexes.emplace(curIdx++);
		}
	}


	void BindlessResourceManager::Initialize(uint64_t frameNum)
	{
		// Note: m_brmMutex must already be held

		LOG("Initializing BindlessResourceManager to manage %d resources", m_platObj->m_currentMaxIndex);

		platform::BindlessResourceManager::Initialize(*this, frameNum);
	}


	void BindlessResourceManager::IncreaseSetSize()
	{
		// Note: m_brmMutex must already be held
		{
			std::lock_guard<std::mutex> lock(m_platObj->m_platformParamsMutex);

			const uint32_t currentNumResources = m_platObj->m_currentMaxIndex;

			m_platObj->m_currentMaxIndex =
				static_cast<uint32_t>(glm::ceil(m_platObj->m_currentMaxIndex * k_growthFactor));

			for (uint32_t curIdx = currentNumResources; curIdx < m_platObj->m_currentMaxIndex; ++curIdx)
			{
				m_freeIndexes.emplace(curIdx);
			}

			m_mustReinitialize = true;

			LOG("BindlessResourceManager resource count increased from %d to %d",
				currentNumResources, m_platObj->m_currentMaxIndex);
		}
	}


	ResourceHandle BindlessResourceManager::RegisterResource(std::unique_ptr<IBindlessResource>&& newBindlessResource)
	{
		{
			std::lock_guard<std::mutex> lock(m_brmMutex);

			if (m_freeIndexes.empty())
			{
				IncreaseSetSize();
			}

			const ResourceHandle resourceIdx = m_freeIndexes.top();
			m_freeIndexes.pop();

			m_registrations.emplace_back(RegistrationMetadata{
				.m_resource = std::move(newBindlessResource),
				.m_resourceHandle = resourceIdx
				});

			return resourceIdx;
		}
	}


	void BindlessResourceManager::UnregisterResource(ResourceHandle& resourceIdx, uint64_t frameNum)
	{
		{
			std::lock_guard<std::mutex> lock(m_brmMutex);

			m_unregistrations.emplace(UnregistrationMetadata{ 
				.m_unregistrationFrameNum = frameNum,
				.m_resourceHandle = resourceIdx,
			});

			resourceIdx = INVALID_RESOURCE_IDX;
		}
	}


	void BindlessResourceManager::Update(uint64_t frameNum)
	{
		{
			std::lock_guard<std::mutex> lock(m_brmMutex);

			if (m_mustReinitialize)
			{
				Initialize(frameNum);

				m_mustReinitialize = false;
			}

			ProcessUnregistrations(frameNum);
			ProcessRegistrations();
		}
	}


	void BindlessResourceManager::ProcessUnregistrations(uint64_t frameNum)
	{
		// Note: m_brmMutex must already be held

		// Release freed resources after N frames have passed:
		while (!m_unregistrations.empty() &&
			m_unregistrations.front().m_unregistrationFrameNum + m_numFramesInFlight < frameNum)
		{
			// Null out the resource:
			platform::BindlessResourceManager::SetResource(*this, nullptr, m_unregistrations.front().m_resourceHandle);

			m_freeIndexes.emplace(m_unregistrations.front().m_resourceHandle);

			m_unregistrations.pop();
		}
	}


	void BindlessResourceManager::ProcessRegistrations()
	{
		// Note: m_brmMutex must already be held

		// Write descriptors for any newly registered resources:
		for (auto& registration : m_registrations)
		{
			platform::BindlessResourceManager::SetResource(
				*this, registration.m_resource.get(), registration.m_resourceHandle);
		}
		m_registrations.clear();
	}


	void BindlessResourceManager::Destroy()
	{
		std::lock_guard<std::mutex> lock(m_brmMutex);

		ProcessUnregistrations(std::numeric_limits<uint64_t>::max()); // Immediately unregister everything

		{
			std::lock_guard<std::mutex> lockPlatObj(m_platObj->m_platformParamsMutex);

			SEAssert(m_freeIndexes.size() == m_platObj->m_currentMaxIndex,
				"Some resource handles have not been returned to the BindlessResourceManager");
		}

		m_freeIndexes = {};

		re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platObj));
	}
}