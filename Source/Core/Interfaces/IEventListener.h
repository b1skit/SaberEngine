// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "../EventManager.h"


namespace core
{
	class IEventListener
	{
	public:
		virtual ~IEventListener() = default;


	protected: // IEventListener interface:
		virtual void HandleEvents() = 0;


	public:
		void PostEvent(core::EventManager::EventInfo const& eventInfo);


	protected:		
		bool HasEvents() const;
		core::EventManager::EventInfo GetEvent();


	private:
		std::queue<core::EventManager::EventInfo> m_events;
		mutable std::shared_mutex m_eventsMutex;
	};


	inline void IEventListener::PostEvent(core::EventManager::EventInfo const& eventInfo)
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


	inline core::EventManager::EventInfo IEventListener::GetEvent()
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_eventsMutex);
			core::EventManager::EventInfo nextEvent = m_events.front();
			m_events.pop();
			return nextEvent;
		}
	}
}