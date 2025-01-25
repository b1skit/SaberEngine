// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "EventManager.h"
#include "EventManager_Platform.h"
#include "Logger.h"

#include "Interfaces/IEventListener.h"


namespace core
{
	EventManager* EventManager::Get()
	{
		static std::unique_ptr<core::EventManager> instance = std::make_unique<core::EventManager>();
		return instance.get();
	}


	EventManager::EventManager()
	{
		m_eventQueue.reserve(1024); // Just a wild guess; we clear this each frame
	}


	void EventManager::Startup()
	{
		LOG("Event manager starting...");
	}


	void EventManager::Shutdown()
	{
		Update(0, 0.0); // Run one last update

		LOG("Event manager shutting down...");
	}


	void EventManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		platform::EventManager::ProcessMessages(*this);

		{
			std::scoped_lock lock(m_eventQueueMutex, m_eventListenersMutex);

			for (auto const& curEvent : m_eventQueue)
			{
				if (m_eventListeners.contains(curEvent.m_eventKey))
				{
					for (auto& listener : m_eventListeners.at(curEvent.m_eventKey))
					{
						listener->RegisterEvent(curEvent);
					}
				}
			}
			m_eventQueue.clear();
		}
	}


	void EventManager::Subscribe(util::CHashKey const& eventType, IEventListener* listener)
	{
		{
			std::lock_guard<std::mutex> lock(m_eventListenersMutex);

			m_eventListeners[eventType].emplace_back(listener);
		}
	}


	void EventManager::Notify(EventInfo&& eventInfo)
	{
		{
			std::lock_guard<std::mutex> lock(m_eventQueueMutex);

			m_eventQueue.emplace_back(std::move(eventInfo));
		}
	}
}