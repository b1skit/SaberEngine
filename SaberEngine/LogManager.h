#pragma once

#include <string>
#include <iostream>
#include <queue>
#include <mutex>
#include <sstream>
#include <array>
#include <stdio.h>
#include <stdarg.h>

#include "EventListener.h"
#include "EngineComponent.h"


namespace en
{
	class LogManager : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		static LogManager* Get(); // Singleton functionality


	public:
		/* Public logging interface:
		----------------------------*/
		template<typename...Args>
		inline static void Log(char const* msg, Args&&... args);

		template<typename... Args>
		inline static void LogWarning(char const* msg, Args&&... args);

		template<typename... Args>
		inline static void LogError(char const* msg, Args&&... args);

	public:
		LogManager();
		~LogManager() = default;

		// Disallow copying of our Singleton
		LogManager(LogManager const&) = delete; 
		LogManager(LogManager&&) = delete;
		void operator=(LogManager const&) = delete;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(const double stepTimeMs) override;

		// EventListener interface:
		void HandleEvents() override;


	private:
		void AddMessage(std::string&& msg);
		std::queue<std::string> m_logMessages;
		size_t m_maxLogLines;
		std::mutex m_logMessagesMutex;


	private:
		struct
		{
			bool m_consoleRequested;
			bool m_consoleReady;
		} m_consoleState;


	private:
		// Static helpers:
		template<typename... Args>
		inline static void LogInternal(char const* tagPrefix, char const* msg, Args&&... args);
		static void AssembleStringFromVariadicArgs(char* buf, uint32_t bufferSize, const char* fmt, ...);
		static std::string FormatStringForLog(char const* prefix, const char* tag, char const* assembledMsg);
	};


	template<typename...Args>
	inline static void LogManager::Log(char const* msg, Args&&... args)
	{
		LogInternal("Log:\t", msg, std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline static void LogManager::LogWarning(char const* msg, Args&&... args)
	{
		LogInternal("Warn:\t", msg, std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline static void LogManager::LogError(char const* msg, Args&&... args)
	{
		LogInternal("Error:\t", msg, std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline static void LogManager::LogInternal(char const* tagPrefix, char const* msg, Args&&... args)
	{
		constexpr uint32_t bufferSize = 128;
		std::array<char, bufferSize> assembledMsg;

		AssembleStringFromVariadicArgs(assembledMsg.data(), bufferSize, msg, args...);

		std::string formattedStr;
		if (msg[0] == '\n')
		{
			formattedStr = FormatStringForLog("\n", tagPrefix, &assembledMsg.data()[1]);
		}
		else if (msg[0] == '\t')
		{
			formattedStr = FormatStringForLog("\t", nullptr, &assembledMsg.data()[1]);
		}
		else
		{
			formattedStr = FormatStringForLog(nullptr, tagPrefix, assembledMsg.data());
		}

		LogManager::Get()->AddMessage(std::move(formattedStr));
	}
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