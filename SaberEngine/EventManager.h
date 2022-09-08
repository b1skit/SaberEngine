// Saber Engine Event Generator

#pragma once
#include "EngineComponent.h"	// Base class

#include <vector>
#include <memory>

#include <SDL.h>

using std::vector;


namespace SaberEngine
{
	// Predeclarations:
	class EventListener;


	const static int EVENT_QUEUE_START_SIZE = 100; // The starting size of the event queue to reserve

	enum EVENT_TYPE
	{
		// System:
		EVENT_ENGINE_QUIT,
		
		// Button inputs:
		EVENT_INPUT_BUTTON_DOWN_FORWARD,
		EVENT_INPUT_BUTTON_UP_FORWARD,
		EVENT_INPUT_BUTTON_DOWN_BACKWARD,
		EVENT_INPUT_BUTTON_UP_BACKWARD,
		EVENT_INPUT_BUTTON_DOWN_LEFT,
		EVENT_INPUT_BUTTON_UP_LEFT,
		EVENT_INPUT_BUTTON_DOWN_RIGHT,
		EVENT_INPUT_BUTTON_UP_RIGHT,
		EVENT_INPUT_BUTTON_DOWN_UP,
		EVENT_INPUT_BUTTON_UP_UP,
		EVENT_INPUT_BUTTON_DOWN_DOWN,
		EVENT_INPUT_BUTTON_UP_DOWN,
		
		// Mouse inputs:
		EVENT_INPUT_MOUSE_CLICK_LEFT,
		EVENT_INPUT_MOUSE_RELEASE_LEFT,
		EVENT_INPUT_MOUSE_CLICK_RIGHT,
		EVENT_INPUT_MOUSE_RELEASE_RIGHT,

		// EVENT_TICK ??
		// EVENT_UPDATE ??
		// ...

		EVENT_NUM_EVENTS // RESERVED: A count of the number of EVENT_TYPE's
	};
	
	// Matched event string names:
	const static std::string EVENT_NAME[EVENT_NUM_EVENTS] =
	{
		// System:
		"EVENT_ENGINE_QUIT", 

		// Button inputs:
		"EVENT_INPUT_BUTTON_DOWN_FORWARD",
		"EVENT_INPUT_BUTTON_UP_FORWARD",
		"EVENT_INPUT_BUTTON_DOWN_BACKWARD",
		"EVENT_INPUT_BUTTON_UP_BACKWARD",
		"EVENT_INPUT_BUTTON_DOWN_LEFT",
		"EVENT_INPUT_BUTTON_UP_LEFT",
		"EVENT_INPUT_BUTTON_DOWN_RIGHT",
		"EVENT_INPUT_BUTTON_UP_RIGHT",
		"EVENT_INPUT_BUTTON_DOWN_UP",
		"EVENT_INPUT_BUTTON_UP_UP",
		"EVENT_INPUT_BUTTON_DOWN_DOWN",
		"EVENT_INPUT_BUTTON_UP_DOWN",

		// Mouse inputs:
		"EVENT_INPUT_MOUSE_CLICK_LEFT",
		"EVENT_INPUT_MOUSE_RELEASE_LEFT",
		"EVENT_INPUT_MOUSE_CLICK_RIGHT",
		"EVENT_INPUT_MOUSE_RELEASE_RIGHT",

	}; // NOTE: String order must match the order of EVENT_TYPE enum


	struct EventInfo
	{
		EVENT_TYPE m_type;
		SaberObject* m_generator;
		std::string m_eventMessage;
	};


	class EventManager : public virtual EngineComponent
	{
	public:
		EventManager();
		~EventManager() = default;
		
		// Singleton functionality:
		static EventManager& Instance();
		EventManager(EventManager const&)	= delete; // Disallow copying of our Singleton
		void operator=(EventManager const&) = delete;
		
		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// Member functions:
		void Subscribe(EVENT_TYPE eventType, EventListener* listener); // Subscribe to an event
		/*void Unsubscribe(EventListener* listener);*/
		void Notify(std::shared_ptr<EventInfo const> eventInfo); // Post an event

	private:
		vector< vector<std::shared_ptr<EventInfo const>>> m_eventQueues;
		vector< vector<EventListener*>> m_eventListeners;
	};


}