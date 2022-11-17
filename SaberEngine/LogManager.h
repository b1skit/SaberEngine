#pragma once

#include <string>
#include <iostream>

#include "EventListener.h"
#include "EngineComponent.h"


namespace en
{
	class LogManager : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		LogManager();
		~LogManager() = default;

		// Singleton functionality:
		static LogManager& Instance();

		// Disallow copying of our Singleton
		LogManager(LogManager const&) = delete; 
		LogManager(LogManager&&) = delete;
		void operator=(LogManager const&) = delete;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// EventListener interface:
		void HandleEvent(en::EventManager::EventInfo const& eventInfo) override;

	private:
		struct
		{
			bool m_consoleRequested;
			bool m_consoleReady;
		} m_consoleState;


	public:
		/* Templated static functions:
		----------------------------*/
		template<typename...Args>
		inline static void Log(char const* msg, Args... args)
		{
			const size_t msgLen = strlen(msg);

			std::unique_ptr<char[]> prefixedMsg;
			if (msg[0] == '\n')
			{
				const size_t totalLen = msgLen + 6 + 1 + 1; // + prefix chars + newline char + null char 
				prefixedMsg = std::make_unique<char[]>(totalLen);
				snprintf(prefixedMsg.get(), totalLen, "\nLog:\t%s%s", &msg[1], "\n");
				printf(prefixedMsg.get(), args...);
			}
			else if (msg[0] == '\t')
			{
				printf(msg, args...);
				printf("\n");
			}
			else
			{
				const size_t totalLen = msgLen + 5 + 1 + 1; // + prefix chars + newline char + null char 
				prefixedMsg = std::make_unique<char[]>(totalLen);
				snprintf(prefixedMsg.get(), totalLen, "Log:\t%s%s", msg, "\n");
				printf(prefixedMsg.get(), args...);
			}
		}


		template<typename... Args>
		inline static void LogWarning(char const* msg, Args&& ... args)
		{
			const size_t msgLen = strlen(msg);
			std::unique_ptr<char[]> prefixedMsg;
			if (msg[0] == '\n')
			{
				const size_t totalLen = msgLen + 7 + 1 + 1; // + prefix chars + newline char + null char 
				prefixedMsg = std::make_unique<char[]>(totalLen);
				snprintf(prefixedMsg.get(), totalLen, "\nWarn:\t%s%s", &msg[1], "\n");
				printf(prefixedMsg.get(), args...);
			}
			else if (msg[0] == '\t')
			{
				printf(msg, args...);
				printf("\n");
			}
			else
			{
				const size_t totalLen = msgLen + 6 + 1 + 1; // + prefix chars + newline char + null char 
				prefixedMsg = std::make_unique<char[]>(totalLen);
				snprintf(prefixedMsg.get(), totalLen, "Warn:\t%s%s", msg, "\n");
				printf(prefixedMsg.get(), args...);
			}
		}


		template<typename... Args>
		inline static void LogError(char const* msg, Args&& ... args)
		{
			const size_t msgLen = strlen(msg);
			std::unique_ptr<char[]> prefixedMsg;
			if (msg[0] == '\n')
			{
				const size_t totalLen = msgLen + 8 + 1 + 1; // + prefix chars + newline char + null char 
				prefixedMsg = std::make_unique<char[]>(totalLen);
				snprintf(prefixedMsg.get(), totalLen, "\nError:\t%s%s", &msg[1], "\n");
				printf(prefixedMsg.get(), args...);
			}
			else if (msg[0] == '\t')
			{
				printf(msg, args...);
				printf("\n");
			}
			else
			{
				const size_t totalLen = msgLen + 7 + 1 + 1; // + prefix chars + newline char + null char 
				prefixedMsg = std::make_unique<char[]>(totalLen);
				snprintf(prefixedMsg.get(), totalLen, "Error:\t%s%s", msg, "\n");
				printf(prefixedMsg.get(), args...);
			}
		}
	};
}


// Log Manager macros:
// ------------------------------------------------

#if defined (_DEBUG)
	#include "LogManager.h"
	#define LOG(msg, ...)			en::LogManager::Log(msg, __VA_ARGS__);
	#define LOG_WARNING(msg, ...)	en::LogManager::LogWarning(msg, __VA_ARGS__);
	#define LOG_ERROR(msg, ...)		en::LogManager::LogError(msg, __VA_ARGS__);
#else
	// Disable compiler warning C4834: discarding return value of function with 'nodiscard' attribute
	#pragma warning(disable : 4834) 
	#define LOG(...)			do {__VA_ARGS__;} while(false);
	#define LOG_WARNING(...)	do {__VA_ARGS__;} while(false);
	#define LOG_ERROR(...)		do {__VA_ARGS__;} while(false);
	// "__VA_ARGS__;" marks our arguments as "used" to the compiler; Avoids "warning C4101: unreferenced local variable"
#endif