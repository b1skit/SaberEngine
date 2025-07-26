// © 2022 Adam Badke. All rights reserved.
#include "EventManager.h"
#include "EventManager_Platform.h"
#include "Logger.h"

#include "Interfaces/IEventListener.h"


namespace core
{
	std::vector<EventManager::EventInfo> EventManager::s_eventQueue;
	std::mutex EventManager::s_eventQueueMutex;

	std::unordered_map<util::CHashKey, std::vector<IEventListener*>> EventManager::s_eventListeners;
	std::mutex EventManager::s_eventListenersMutex;


	void EventManager::Startup()
	{
		LOG("Event manager starting...");

		s_eventQueue.reserve(1024); // Just a wild guess; we clear this each frame
	}


	void EventManager::Shutdown()
	{
		Update(); // Run one last update

		LOG("Event manager shutting down...");
	}


	void EventManager::Update()
	{
		platform::EventManager::ProcessMessages();

		{
			std::scoped_lock lock(s_eventQueueMutex, s_eventListenersMutex);

			for (auto const& curEvent : s_eventQueue)
			{
				if (s_eventListeners.contains(curEvent.m_eventKey))
				{
					for (auto& listener : s_eventListeners.at(curEvent.m_eventKey))
					{
						listener->PostEvent(curEvent);
					}
				}
			}
			s_eventQueue.clear();
		}
	}


	void EventManager::Subscribe(util::CHashKey const& eventType, IEventListener* listener)
	{
		{
			std::lock_guard<std::mutex> lock(s_eventListenersMutex);

			s_eventListeners[eventType].emplace_back(listener);
		}
	}


	void EventManager::Notify(EventInfo&& eventInfo)
	{
		{
			std::lock_guard<std::mutex> lock(s_eventQueueMutex);

			s_eventQueue.emplace_back(std::move(eventInfo));
		}
	}
}