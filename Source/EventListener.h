// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "EventManager.h"


namespace en
{
	class IEventListener
	{
	public: // IEventListener interface:
		virtual void HandleEvents() = 0;


	public:
		void RegisterEvent(en::EventManager::EventInfo const& eventInfo);
		bool HasEvents() const;
		en::EventManager::EventInfo GetEvent();


	private:
		std::queue<en::EventManager::EventInfo> m_events;
		mutable std::shared_mutex m_eventsMutex;
	};


	inline void IEventListener::RegisterEvent(en::EventManager::EventInfo const& eventInfo)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_eventsMutex);
			m_events.emplace(eventInfo);
		}
	}

	
	inline bool IEventListener::HasEvents() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_eventsMutex);
			return !m_events.empty();
		}
	}


	inline en::EventManager::EventInfo IEventListener::GetEvent()
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_eventsMutex);
			en::EventManager::EventInfo nextEvent = m_events.front();
			m_events.pop();
			return nextEvent;
		}
	}
}