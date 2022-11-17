#pragma once

#include <queue>

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
	};


	void EventListener::RegisterEvent(en::EventManager::EventInfo const& eventInfo)
	{
		m_events.emplace(eventInfo);
	}

	
	bool EventListener::HasEvents() const
	{ 
		return !m_events.empty();
	}

	en::EventManager::EventInfo EventListener::GetEvent()
	{
		en::EventManager::EventInfo nextEvent = m_events.front();
		m_events.pop();
		return nextEvent;
	}
}