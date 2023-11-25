#pragma once
#include "EventListener.h"


namespace fr
{
	class EventListenerComponent
	{
	public:
		EventListenerComponent(void(*handleEventsImpl)())
			: m_eventListener(handleEventsImpl)
		{
		}

		void AddEventSubscription(en::EventManager::EventType);

		inline void HandleEvents() { m_eventListener.HandleEvents(); }

		inline bool HasEvents() const { return m_eventListener.HasEvents(); }

		inline en::EventManager::EventInfo GetEvent() { return m_eventListener.GetEvent(); }


	private:
		struct EventListenerWrapper final : public virtual en::EventListener
		{
		public:
			EventListenerWrapper(void(*handleEventsImpl)()) { HandleEventsImpl = handleEventsImpl; }

			void HandleEvents() { HandleEventsImpl(); }

		private:
			void (*HandleEventsImpl)() = nullptr;
		} m_eventListener;
	};


	void EventListenerComponent::AddEventSubscription(en::EventManager::EventType eventType)
	{
		en::EventManager::Get()->Subscribe(eventType, &m_eventListener);
	}
}