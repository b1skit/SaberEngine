#include <string>
using std::to_string;

#include <iostream>
using std::cout;

#include "LogManager.h"
#include "EventManager.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"


namespace en
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
			CoreEngine::GetEventManager()->Subscribe(InputButtonDown_Forward, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonUp_Forward, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonDown_Backward, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonUp_Backward, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonDown_Left, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonUp_Left, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonDown_Right, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonUp_Right, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonDown_Up, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonUp_Up, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonDown_Down, this);
			CoreEngine::GetEventManager()->Subscribe(InputButtonUp_Down, this);
			LOG("\tKey input logging enabled");
		#endif

		#if defined(DEBUG_LOGMANAGER_MOUSE_INPUT_LOGGING)
			CoreEngine::GetEventManager()->Subscribe(InputMouseClick_Left, this);
			CoreEngine::GetEventManager()->Subscribe(InputMouseRelease_Left, this);
			CoreEngine::GetEventManager()->Subscribe(InputMouseClick_Right, this);
			CoreEngine::GetEventManager()->Subscribe(InputMouseRelease_Right, this);
			CoreEngine::GetEventManager()->Subscribe(EVENT_INPUT_MOUSE_MOVED, this);
			LOG("\tMouse input logging enabled");
		#endif

		#if defined(DEBUG_LOGMANAGER_QUIT_LOGGING)
			CoreEngine::GetEventManager()->Subscribe(EngineQuit, this);
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


	void LogManager::HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo)
	{
		#if defined(DEBUG_LOGMANAGER_LOG_EVENTS)
			string logMessage = EventName[eventInfo->m_type] + ": Object #";

			if (eventInfo->m_generator)
			{
				logMessage += std::to_string(eventInfo->m_generator->GetUniqueID()) + " (" + eventInfo->m_generator->GetName() + ")\t";
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