#pragma once

#include <queue>
#include <mutex>

#include "EventManager.h"


namespace en
{
	class EventListener
	{
	public:
		virtual void HandleEvents() = 0;

		inline void RegisterEvent(en::EventManager::EventInfo const& eventInfo);
		inline bool HasEvents() const;
		inline en::EventManager::EventInfo GetEvent();

	private:
		std::queue<en::EventManager::EventInfo> m_events;
		mutable std::mutex m_eventsMutex;
	};


	void EventListener::RegisterEvent(en::EventManager::EventInfo const& eventInfo)
	{
		std::lock_guard<std::mutex> lock(m_eventsMutex);
		m_events.emplace(eventInfo);
	}

	
	bool EventListener::HasEvents() const
	{
		std::lock_guard<std::mutex> lock(m_eventsMutex);
		return !m_events.empty();
	}

	en::EventManager::EventInfo EventListener::GetEvent()
	{
		std::lock_guard<std::mutex> lock(m_eventsMutex);
		en::EventManager::EventInfo nextEvent = m_events.front();
		m_events.pop();
		return nextEvent;
	}
}