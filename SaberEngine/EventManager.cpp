// Handles logging for the engine and 

#include "EventManager.h"
#include "SaberObject.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "EventListener.h"

#include <SDL.h>


namespace SaberEngine
{
	EventManager::EventManager() : EngineComponent("EventManager")
	{
		m_eventQueues.reserve(EVENT_NUM_EVENTS);
		for (int i = 0; i < EVENT_NUM_EVENTS; i++)
		{
			m_eventQueues.push_back(vector<EventInfo const*>());
		}

		m_eventListeners.reserve(EVENT_QUEUE_START_SIZE);
		for (int i = 0; i < EVENT_QUEUE_START_SIZE; i++)
		{
			m_eventListeners.push_back(vector<EventListener*>());
		}		
	}


	//EventManager::~EventManager()
	//{
	//
	//}


	EventManager& EventManager::Instance()
	{
		static EventManager* instance = new EventManager();
		return *instance;
	}


	void EventManager::Startup()
	{
		LOG("Event manager started!");
	}


	void EventManager::Shutdown()
	{
		Update(); // Run one last update

		LOG("Event manager shutting down...");
	}


	void EventManager::Update()
	{
		// Check for SDL quit events (only). We do this instead of parsing the entire queue with SDL_PollEvent(), which removed input events we needed
		SDL_PumpEvents();
		
		#define NUM_EVENTS 1
		SDL_Event eventBuffer[NUM_EVENTS]; // 
		if (SDL_PeepEvents(eventBuffer, NUM_EVENTS, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) > 0)
		{
			Notify(new EventInfo{ EVENT_ENGINE_QUIT, this, new string("Received SDL_QUIT event") });
		}

		// Loop through each type of event:
		for (int currentEventType = 0; currentEventType < EVENT_NUM_EVENTS; currentEventType++)
		{
			// Loop through each event item in the current event queue:
			size_t numCurrentEvents = m_eventQueues[currentEventType].size();
			for (int currentEvent = 0; currentEvent < numCurrentEvents; currentEvent++)
			{
				// Loop through each listener subscribed to the current event:
				size_t numListeners = m_eventListeners[currentEventType].size();
				for (int currentListener = 0; currentListener < numListeners; currentListener++)
				{
					m_eventListeners[currentEventType][currentListener]->HandleEvent(m_eventQueues[currentEventType][currentEvent]);
				}
				
				// Deallocate the event:
				if (m_eventQueues[currentEventType][currentEvent]->m_eventMessage != nullptr)
				{
					delete m_eventQueues[currentEventType][currentEvent]->m_eventMessage;
				}
				delete m_eventQueues[currentEventType][currentEvent];
			}

			// Clear the current event queue (of now invalid pointers):
			m_eventQueues[currentEventType].clear();
		}
	}


	void EventManager::Subscribe(EVENT_TYPE eventType, EventListener* listener)
	{
		m_eventListeners[eventType].push_back(listener);
		return;
	}


	//void EventManager::Unsubscribe(EventListener * listener)
	//{
	//	// DEBUG:
	//	Notify(EventInfo{ EVENT_ERROR, this, "EventManager.Unsubscribe() was called, but is NOT implemented!"});

	//	return;
	//}


	void EventManager::Notify(EventInfo const* eventInfo, bool pushToFront /*= false*/)
	{
		#if defined(DEBUG_PRINT_NOTIFICATIONS)
			if (eventInfo)
			{
				if (eventInfo->m_generator)
				{
					if (eventInfo->m_eventMessage)
					{
						LOG("NOTIFICATION: " + to_string((long long)eventInfo->m_generator) + " : " + *eventInfo->m_eventMessage);
					}
					else
					{
						LOG("NOTIFICATION: " + to_string((long long)eventInfo->m_generator) + " : nullptr");
					}
				}
				else
				{
					if (eventInfo->m_eventMessage)
					{
						LOG("NOTIFICATION: nullptr : " + *eventInfo->m_eventMessage);
					}
					else
					{
						LOG("NOTIFICATION: nullptr : nullptr");
					}
				}
			}
			else
			{
				LOG("NOTIFICATION: Received NULL eventInfo...");
			}			
		#endif

		// Select what to notify based on type?

		if (pushToFront)
		{
			vector<EventInfo const*>::iterator iterator = m_eventQueues[(int)eventInfo->m_type].begin();
			m_eventQueues[(int)eventInfo->m_type].insert(iterator, eventInfo);
		}
		else
		{
			m_eventQueues[(int)eventInfo->m_type].push_back(eventInfo);
		}
		return;
	}
}