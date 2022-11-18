#include "EventManager.h"
#include "NamedObject.h"
#include "DebugConfiguration.h"
#include "EventListener.h"
#include "Context.h"
#include "RenderManager.h"

#include <SDL.h>

using std::vector;


namespace en
{
	EventManager* EventManager::Get()
	{
		static std::unique_ptr<en::EventManager> instance = std::make_unique<en::EventManager>();
		return instance.get();
	}


	EventManager::EventManager()
	{
		m_eventQueues.reserve(EventType_Count);
		for (uint32_t i = 0; i < EventType_Count; i++)
		{
			m_eventQueues.push_back(vector<EventInfo>());
		}

		constexpr size_t eventQueueStartSize = 100; // The starting size of the event queue to reserve
		m_eventListeners.reserve(eventQueueStartSize);
		for (uint32_t i = 0; i < eventQueueStartSize; i++)
		{
			m_eventListeners.push_back(vector<EventListener*>());
		}		
	}


	void EventManager::Startup()
	{
		LOG("Event manager starting...");
	}


	void EventManager::Shutdown()
	{
		Update(0.0); // Run one last update

		LOG("Event manager shutting down...");
	}


	void EventManager::Update(const double stepTimeMs)
	{
		// NOTE: SDL event handling must be run on the same thread that initialized the video subsystem (ie. main), as
		// it may implicitely call SDL_PumpEvents()
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			EventInfo eventInfo;
			bool doBroadcastEvent = true;

			switch (event.type)
			{
			case SDL_QUIT:
			{
				// Note: This is called when the user manually quits the program (eg. by clicking the close "X" button)
				// This is different to the SaberEngine InputButton_Quit event
				eventInfo.m_type = EngineQuit;
			}
			break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				eventInfo.m_type = KeyEvent;
				// Pack the data: m_data0.m_dataUI = SDL_Scancode, m_data0.m_dataB = button state up/down (T/F)
				eventInfo.m_data0.m_dataUI = event.key.keysym.scancode;
				eventInfo.m_data1.m_dataB = event.type == SDL_KEYDOWN ? true : false;
			}
			break;
			case SDL_MOUSEMOTION:
			{
				eventInfo.m_type = MouseMotionEvent;
				eventInfo.m_data0.m_dataI = event.motion.xrel;
				eventInfo.m_data1.m_dataI = event.motion.yrel;
			}
			break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				eventInfo.m_type = MouseButtonEvent;
				// Pack the data: 
				// m_data0.m_dataUI = button index (0/1/2 = L/M/R)
				// m_data1.m_dataUB = button state (T/F = pressed/released)
				switch (event.button.button)
				{
				case SDL_BUTTON_LEFT:
				{
					eventInfo.m_data0.m_dataUI = 0;
				}
				break;
				case SDL_BUTTON_MIDDLE:
				{
					eventInfo.m_data0.m_dataUI = 1;
				}
				case SDL_BUTTON_RIGHT:
				{
					eventInfo.m_data0.m_dataUI = 2;
				}
				break;
				}
				eventInfo.m_data1.m_dataB = event.button.state == SDL_PRESSED ? true : false;
			}
			break;
			case SDL_MOUSEWHEEL:
			{
				// TODO...
				doBroadcastEvent = false;
			}
			break;
			default:
				doBroadcastEvent = false;
			}

			// Only broadcast the event if it has been populated by something we're interested in
			if (doBroadcastEvent)
			{
				Notify(eventInfo);
			}
		}

		// Loop through each type of event:
		for (size_t currentEventType = 0; currentEventType < EventType_Count; currentEventType++)
		{
			// Loop through each event item in the current event queue:
			size_t numCurrentEvents = m_eventQueues[currentEventType].size();
			for (size_t currentEvent = 0; currentEvent < numCurrentEvents; currentEvent++)
			{
				// Loop through each listener subscribed to the current event:
				size_t numListeners = m_eventListeners[currentEventType].size();
				for (size_t currentListener = 0; currentListener < numListeners; currentListener++)
				{
					m_eventListeners[currentEventType][currentListener]->RegisterEvent(
						m_eventQueues[currentEventType][currentEvent]);
				}
			}

			// Clear the current event queue:
			m_eventQueues[currentEventType].clear();
		}
	}


	void EventManager::Subscribe(EventType eventType, EventListener* listener)
	{
		m_eventListeners[eventType].push_back(listener);
		return;
	}


	void EventManager::Notify(EventInfo const& eventInfo)
	{
		m_eventQueues[(size_t)eventInfo.m_type].emplace_back(eventInfo);
	}
}