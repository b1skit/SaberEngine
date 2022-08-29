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
			m_eventQueues.push_back(vector<std::shared_ptr<EventInfo const>>());
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
			Notify(std::make_shared<EventInfo const>(
				EventInfo({ 
					EVENT_ENGINE_QUIT, 
					this, 
					"Received SDL_QUIT event" })));
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
				m_eventQueues[currentEventType][currentEvent] = nullptr;
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


	void EventManager::Notify(std::shared_ptr<EventInfo const> eventInfo)
	{
		SEAssert("Event generator is null", eventInfo->m_generator != nullptr);
		SEAssert("Event message is empty", !eventInfo->m_eventMessage.empty());

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

		m_eventQueues[(int)eventInfo->m_type].push_back(eventInfo);

	}
}