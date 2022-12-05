#pragma once

#include <vector>
#include <memory>
#include <string>
#include <mutex>

#include "EngineComponent.h"


namespace en
{
	class EventListener;
}
namespace re
{
	class Context;
}


namespace en
{
	class EventManager : public virtual en::EngineComponent
	{
	public:
		enum EventType
		{
			// Generic events: These will likely have packed data that needs to be interpreted
			KeyEvent,
			MouseMotionEvent,
			MouseButtonEvent,

			// Functionality triggers: Typically a system will be interested in these, not specific button states
			InputForward,
			InputBackward,
			InputLeft,
			InputRight,
			InputUp,
			InputDown,
			InputSprint,

			// System:
			InputToggleConsole,

			EngineQuit,

			// Mouse functions. TODO: These should be named w.r.t what they actually do:
			InputMouseLeft,
			InputMouseRight,

			EventType_Count
		};

		// Matched event string names:
		const static std::string EventName[EventType_Count];


		typedef union { float m_dataF; int32_t m_dataI; uint32_t m_dataUI; bool m_dataB; } EventData;
		struct EventInfo
		{
			EventType m_type;
			EventData m_data0;
			EventData m_data1;
		};


	public:
		static EventManager* Get(); // Singleton functionality

	public:
		EventManager();
		~EventManager() = default;
		
		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(const double stepTimeMs) override;

		// Member functions:
		void Subscribe(EventType eventType, EventListener* listener); // Subscribe to an event
		void Notify(EventInfo const& eventInfo); // Post an event


	private:
		std::vector<std::vector<EventInfo>> m_eventQueues;
		std::vector<std::vector<EventListener*>> m_eventListeners;
		std::mutex m_eventMutex;

	private:
		EventManager(EventManager const&) = delete;
		EventManager(EventManager&&) = delete;
		void operator=(EventManager const&) = delete;
	};


}