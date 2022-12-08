#include <string>
#include <iostream>
#include <memory>

#include "imgui.h"

#include "LogManager.h"
#include "EventManager.h"
#include "DebugConfiguration.h"
#include "Command.h"
#include "RenderManager.h"

using en::EventManager;
using std::make_shared;


namespace
{
	class DisplayConsoleCommand : public virtual en::Command
	{
	public:
		DisplayConsoleCommand(bool* consoleOpen) : m_consoleOpen(consoleOpen) {}

		void Execute() override
		{
			ImGui::ShowDemoWindow(m_consoleOpen);
		}

	private:
		bool* m_consoleOpen;
	};
}

namespace en
{
	LogManager* LogManager::Get()
	{
		static std::unique_ptr<en::LogManager> instance = std::make_unique<en::LogManager>();
		return instance.get();
	}


	LogManager::LogManager()
		: m_consoleState{false, true} // Starting state = "not requested" and "ready"
		, m_maxLogLines(1000) // TODO: Make this controllable via the config.cfg
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


	void LogManager::Update(const double stepTimeMs)
	{
		HandleEvents();

		// Users can open the console by pressing a key, but can close it by pressing the same key again, or by clicking
		// the [x] button to close it. We track the m_consoleRequested status (which toggles each time the users taps
		// the console key) to determine if we're in an open/closed console state. We track the m_consoleReady state
		// to catch when a user clicks [x] to close the window
		if (m_consoleState.m_consoleRequested == true && m_consoleState.m_consoleReady == true)
		{
			re::RenderManager::Get()->EnqueueImGuiCommand(
				make_shared<DisplayConsoleCommand>(&m_consoleState.m_consoleReady));
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
			en::EventManager::EventInfo const& eventInfo = GetEvent();

			if (eventInfo.m_type == EventManager::EventType::InputToggleConsole && eventInfo.m_data0.m_dataB == true)
			{
				m_consoleState.m_consoleRequested = !m_consoleState.m_consoleRequested;
			}
		}	
	}


	void LogManager::AddMessage(std::string&& msg)
	{
		std::lock_guard<std::mutex> lock(m_logMessagesMutex);

		if (m_logMessages.size() >= m_maxLogLines)
		{
			m_logMessages.pop();
		}
		m_logMessages.emplace(std::forward<std::string>(msg));

	#if defined(_DEBUG)
		// Print the message to the terminal while we're holding the lock, to ensure the same ordering as the log queue
		printf(m_logMessages.back().c_str());
	#endif
	}


	void LogManager::AssembleStringFromVariadicArgs(char* buf, uint32_t bufferSize, const char* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		const int numChars = vsprintf_s(buf, bufferSize, msg, args);
		SEAssert("Message is larger than the buffer size; it will be truncated",
			static_cast<uint32_t>(numChars) < bufferSize);
		va_end(args);
	}


	std::string LogManager::FormatStringForLog(char const* prefix, const char* tag, char const* assembledMsg)
	{
		std::ostringstream stream;
		if (prefix)
		{
			stream << prefix;
		}
		if (tag)
		{
			stream << tag;
		}
		stream << assembledMsg << "\n";

		return stream.str();
	}
}