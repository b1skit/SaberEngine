#pragma once

#include <vector>
#include <memory>
#include <string>

#include "EngineComponent.h"

namespace en
{
	class EventListener;
}


namespace en
{
	class EventManager : public virtual en::EngineComponent
	{
	public:
		enum EventType
		{
			// System:
			EngineQuit,

			// Button inputs:
			InputButtonDown_Forward,
			InputButtonUp_Forward,
			InputButtonDown_Backward,
			InputButtonUp_Backward,
			InputButtonDown_Left,
			InputButtonUp_Left,
			InputButtonDown_Right,
			InputButtonUp_Right,
			InputButtonDown_Up,
			InputButtonUp_Up,
			InputButtonDown_Down,
			InputButtonUp_Down,

			// Mouse inputs:
			InputMouseClick_Left,
			InputMouseRelease_Left,
			InputMouseClick_Right,
			InputMouseRelease_Right,

			// EventTick ??
			// EventUpdate ??
			// ...

			EventType_Count // RESERVED: A count of the number of EventType's
		};

		// Matched event string names:
		const static std::string EventName[EventType_Count];

		struct EventInfo
		{
			EventType m_type;
			en::EngineComponent* m_generator;
			std::string m_eventMessage;
		};


	public: // Singleton functionality:		
		static EventManager* Get();
	private:
		static std::unique_ptr<EventManager> m_instance;

	public:
		EventManager();
		~EventManager() = default;
		
		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// Member functions:
		void Subscribe(EventType eventType, EventListener* listener); // Subscribe to an event
		void Notify(std::shared_ptr<EventInfo const> eventInfo); // Post an event


	private:
		std::vector<std::vector<std::shared_ptr<EventInfo const>>> m_eventQueues;
		std::vector<std::vector<EventListener*>> m_eventListeners;


	private:
		EventManager(EventManager const&) = delete;
		EventManager(EventManager&&) = delete;
		void operator=(EventManager const&) = delete;
	};


}