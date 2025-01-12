// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "EventManager.h"
#include "EventManager_Platform.h"
#include "LogManager.h"

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
		//
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
			std::lock_guard<std::mutex> lock(m_eventMutex);

			// Loop through each type of event:
			for (auto const& eventType : m_eventQueues)
			{
				for (auto const& eventInfo : eventType.second)
				{
					if (m_eventListeners.contains(eventInfo.m_eventKey))
					{
						for (auto& listener : m_eventListeners.at(eventInfo.m_eventKey))
						{
							listener->RegisterEvent(eventInfo);
						}
					}
				}
			}
			m_eventQueues.clear();
		}
	}


	void EventManager::Subscribe(util::HashKey const& eventType, IEventListener* listener)
	{
		{
			std::lock_guard<std::mutex> lock(m_eventMutex);

			m_eventListeners[eventType].emplace_back(listener);
		}
	}


	void EventManager::Notify(EventInfo&& eventInfo)
	{
		{
			std::lock_guard<std::mutex> lock(m_eventMutex);

			m_eventQueues[eventInfo.m_eventKey].emplace_back(std::move(eventInfo));
		}
	}
}