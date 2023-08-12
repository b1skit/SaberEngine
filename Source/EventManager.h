// © 2022 Adam Badke. All rights reserved.
#pragma once

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
	class EventManager final : public virtual en::EngineComponent
	{
	public:
		enum EventType : uint8_t
		{
			// Generic events: These will likely have packed data that needs to be interpreted
			KeyEvent,
			MouseMotionEvent,
			MouseButtonEvent,
			MouseWheelEvent,
			TextInputEvent,

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
			InputToggleVSync,

			EngineQuit,

			// Mouse functions:
			InputMouseLeft,
			InputMouseMiddle,
			InputMouseRight,

			Uninitialized, // Error
			EventType_Count = Uninitialized
		};

		// Matched event string names:
		const static std::string EventName[EventType_Count];


		typedef union { float m_dataF; int32_t m_dataI; uint32_t m_dataUI; bool m_dataB; char m_dataC; } EventData;
		struct EventInfo
		{
			EventType m_type = Uninitialized;
			EventData m_data0;
			EventData m_data1;
		};


	public:
		static EventManager* Get(); // Singleton functionality

	public:
		EventManager();
		EventManager(EventManager&&) = default;
		EventManager& operator=(EventManager&&) = default;
		~EventManager() = default;
		
		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// Member functions:
		void Subscribe(EventType eventType, EventListener* listener); // Subscribe to an event
		void Notify(EventInfo const& eventInfo); // Post an event


	private:
		std::vector<std::vector<EventInfo>> m_eventQueues;
		std::vector<std::vector<EventListener*>> m_eventListeners;
		std::mutex m_eventMutex;

	private:
		EventManager(EventManager const&) = delete;
		void operator=(EventManager const&) = delete;
	};


}