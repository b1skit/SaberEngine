#include <string>
using std::to_string;

#include <iostream>
using std::cout;

#include "LogManager.h"
#include "EventManager.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"


namespace fr
{
	LogManager& LogManager::Instance()
	{
		static LogManager* instance = new LogManager();
		return *instance;
	}

	void LogManager::Startup() 
	{
		LOG("Log manager starting...");

		#if defined(DEBUG_LOGMANAGER_KEY_INPUT_LOGGING)
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_DOWN_FORWARD, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_UP_FORWARD, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_DOWN_BACKWARD, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_UP_BACKWARD, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_DOWN_LEFT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_UP_LEFT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_DOWN_RIGHT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_UP_RIGHT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_DOWN_UP, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_UP_UP, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_DOWN_DOWN, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_BUTTON_UP_DOWN, this);
			LOG("\tKey input logging enabled");
		#endif

		#if defined(DEBUG_LOGMANAGER_MOUSE_INPUT_LOGGING)
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_MOUSE_CLICK_LEFT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_MOUSE_RELEASE_LEFT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_MOUSE_CLICK_RIGHT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_MOUSE_RELEASE_RIGHT, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_MOUSE_MOVED, this);
			LOG("\tMouse input logging enabled");
		#endif

		#if defined(DEBUG_LOGMANAGER_QUIT_LOGGING)
			CoreEngine::GetEventManager()->Subscribe(EVENT_ENGINE_QUIT, this);
			LOG("\tQuit event logging enabled");
		#endif
	}


	void LogManager::Shutdown()
	{
		LOG("Log manager shutting down...");
	}


	void LogManager::Update()
	{
	}


	void LogManager::HandleEvent(SaberEngine::EventInfo const* eventInfo)
	{
		#if defined(DEBUG_LOGMANAGER_LOG_EVENTS)
			string logMessage = EVENT_NAME[eventInfo->m_type] + ": Object #";

			if (eventInfo->m_generator)
			{
				logMessage += std::to_string(eventInfo->m_generator->GetObjectID()) + " (" + eventInfo->m_generator->GetName() + ")\t";
			}
			else
			{
				logMessage += "anonymous (     ??    )\t";
			}

			if (eventInfo->m_eventMessage && eventInfo->m_eventMessage->length() > 0)
			{
				logMessage += ": " + *eventInfo->m_eventMessage;
			}

			LOG(logMessage);
		#endif		
		
		return;
	}
}