// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "EventManager.h"


namespace en
{
	class EventListener
	{
	public: // EventListener interface:
		virtual void HandleEvents() = 0;


	public:
		inline void RegisterEvent(en::EventManager::EventInfo const& eventInfo);
		inline bool HasEvents() const;
		inline en::EventManager::EventInfo GetEvent();


	private:
		std::queue<en::EventManager::EventInfo> m_events;
		mutable std::shared_mutex m_eventsMutex;
	};


	void EventListener::RegisterEvent(en::EventManager::EventInfo const& eventInfo)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_eventsMutex);
			m_events.emplace(eventInfo);
		}
	}

	
	bool EventListener::HasEvents() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_eventsMutex);
			return !m_events.empty();
		}
	}


	en::EventManager::EventInfo EventListener::GetEvent()
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_eventsMutex);
			en::EventManager::EventInfo nextEvent = m_events.front();
			m_events.pop();
			return nextEvent;
		}
	}
}