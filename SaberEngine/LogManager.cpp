#include <string>
#include <iostream>

#include "imgui.h"

#include "LogManager.h"
#include "EventManager.h"
#include "DebugConfiguration.h"

using en::EventManager;


namespace en
{
	LogManager& LogManager::Instance()
	{
		static LogManager* instance = new LogManager();
		return *instance;
	}


	LogManager::LogManager()
		: m_consoleState{false, true} // Starting state = "not requested" and "ready"
	{
	}


	void LogManager::Startup() 
	{
		LOG("Log manager starting...");

		// Event subscriptions:
		EventManager::Get()->Subscribe(EventManager::InputToggleConsole, this);
	}


	void LogManager::Shutdown()
	{
		LOG("Log manager shutting down...");
	}


	void LogManager::Update()
	{
		HandleEvents();

		// Users can open the console by pressing a key, but can close it by pressing the same key again, or by clicking
		// the [x] button to close it. We track the m_consoleRequested status (which toggles each time the users taps
		// the console key) to determine if we're in an open/closed console state. We track the m_consoleReady state
		// to catch when a user clicks [x] to close the window
		if (m_consoleState.m_consoleRequested == true && m_consoleState.m_consoleReady == true)
		{
			ImGui::ShowDemoWindow(&m_consoleState.m_consoleReady);
		}
		else if (m_consoleState.m_consoleRequested == true && m_consoleState.m_consoleReady == false)
		{
			EventManager::EventInfo logClosedEvent;
			logClosedEvent.m_type = EventManager::EventType::InputToggleConsole;
			logClosedEvent.m_data0.m_dataB = false;

			EventManager::Get()->Notify(EventManager::EventInfo(logClosedEvent));

			m_consoleState.m_consoleRequested = !m_consoleState.m_consoleRequested;
			m_consoleState.m_consoleReady = true;
		}
	}


	void LogManager::HandleEvents()
	{
		while (HasEvents())
		{
			en::EventManager::EventInfo eventInfo = GetEvent();

			if (eventInfo.m_type == EventManager::EventType::InputToggleConsole && eventInfo.m_data0.m_dataB == true)
			{
				m_consoleState.m_consoleRequested = !m_consoleState.m_consoleRequested;
			}
		}	
	}
}