// © 2022 Adam Badke. All rights reserved.
#include "EventManager.h"
#include "Assert.h"
#include "EventListener.h"
#include "EventManager_Platform.h"


namespace en
{
	using std::vector;


	EventManager* EventManager::Get()
	{
		static std::unique_ptr<en::EventManager> instance = std::make_unique<en::EventManager>();
		return instance.get();
	}


	EventManager::EventManager()
	{
		std::lock_guard<std::mutex> lock(m_eventMutex);

		m_eventQueues.reserve(EventType_Count);
		for (uint32_t i = 0; i < EventType_Count; i++)
		{
			m_eventQueues.push_back(vector<EventInfo>());
		}

		constexpr size_t eventQueueStartSize = 100; // The starting size of the event queue to reserve
		m_eventListeners.reserve(eventQueueStartSize);
		for (uint32_t i = 0; i < eventQueueStartSize; i++)
		{
			m_eventListeners.push_back(vector<EventListener*>());
		}
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

		std::lock_guard<std::mutex> lock(m_eventMutex);

		// Loop through each type of event:
		for (size_t currentEventType = 0; currentEventType < EventType_Count; currentEventType++)
		{
			// Loop through each event item in the current event queue:
			size_t numCurrentEvents = m_eventQueues[currentEventType].size();
			for (size_t currentEvent = 0; currentEvent < numCurrentEvents; currentEvent++)
			{
				// Loop through each listener subscribed to the current event:
				size_t numListeners = m_eventListeners[currentEventType].size();
				for (size_t currentListener = 0; currentListener < numListeners; currentListener++)
				{
					m_eventListeners[currentEventType][currentListener]->RegisterEvent(
						m_eventQueues[currentEventType][currentEvent]);
				}
			}

			// Clear the current event queue:
			m_eventQueues[currentEventType].clear();
		}
	}


	void EventManager::Subscribe(EventType eventType, EventListener* listener)
	{
		std::lock_guard<std::mutex> lock(m_eventMutex);
		m_eventListeners[eventType].emplace_back(listener);
	}


	void EventManager::Notify(EventInfo&& eventInfo)
	{
		std::lock_guard<std::mutex> lock(m_eventMutex);
		m_eventQueues[(size_t)eventInfo.m_type].emplace_back(std::move(eventInfo));
	}
}