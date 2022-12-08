#pragma once

#include <string>
#include <iostream>
#include <queue>
#include <mutex>
#include <sstream>
#include <array>

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
		inline static void Log(char const* msg, Args&&... args)
		{
			LogInternal("Log:\t", msg, std::forward<Args>(args)...);
		}


		template<typename... Args>
		inline static void LogWarning(char const* msg, Args&&... args)
		{
			LogInternal("Warn:\t", msg, std::forward<Args>(args)...);
		}


		template<typename... Args>
		inline static void LogError(char const* msg, Args&&... args)
		{
			LogInternal("Error:\t", msg, std::forward<Args>(args)...);
		}

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
		// Private logging implementation:
		template<typename... Args>
		inline static void LogInternal(char const* tagPrefix, char const* msg, Args&&... args)
		{
			const size_t msgLen = strlen(msg);

			std::string formattedStr;
			if (msg[0] == '\n')
			{
				formattedStr = FormatStringArgs("\n", tagPrefix, &msg[1], std::forward<Args>(args)...);
			}
			else if (msg[0] == '\t')
			{
				formattedStr = FormatStringArgs("\t", nullptr, &msg[1], std::forward<Args>(args)...);
			}
			else
			{
				formattedStr = FormatStringArgs(nullptr, tagPrefix, msg, std::forward<Args>(args)...);
			}

		#if defined(_DEBUG)
			printf(formattedStr.c_str());
		#endif
			LogManager::Get()->AddMessage(std::move(formattedStr));
		}


		// Static helpers:
		private:
			template<typename T>
			static std::string ConvertArg(T arg)
			{
				std::ostringstream oss;
				oss << arg;
				return oss.str();
			}

			template<>
			static std::string ConvertArg<uint8_t>(uint8_t arg)
			{
				std::ostringstream oss;
				oss << (uint16_t)arg; // Stringstream always treats uint8_t as a char; override it here
				return oss.str();
			}

			template <size_t numArgs>
			static inline void RecursiveArgsToStr(std::array<std::string, numArgs>& argStrings)
			{
				// Do nothing; base case
			}

			template <size_t numArgs, typename argType, typename... Args>
			static inline void RecursiveArgsToStr(
				std::array<std::string, numArgs>& argStrings, argType&& curArg, Args&&...arguments)
			{
				argStrings[numArgs - 1 - sizeof...(Args)] = std::move(ConvertArg(curArg));
				RecursiveArgsToStr(argStrings, std::forward<Args>(arguments)...);
			}

			template<typename...Args>
			static inline std::string FormatStringArgs(
				char const* prefix, const char* tag, char const* msg, Args&&... args)
			{
				const size_t msgLen = strlen(msg);
				const size_t numArgs = sizeof...(args);
				std::array<std::string, numArgs> argStrings;
				RecursiveArgsToStr(argStrings, std::forward<Args>(args)...);

				std::ostringstream stream;
				if (prefix)
				{
					stream << prefix;
				}
				if (tag)
				{
					stream << tag;
				}
				size_t argIdx = 0;
				for (size_t i = 0; i < msgLen; i++)
				{
					if (msg[i] == '%' && (i + 1) < msgLen && argIdx < numArgs)
					{
						stream << argStrings[argIdx++];
						i++;
						// TODO: Handle multi-char format specifiers (eg. size_t == %zu)
					}
					else
					{
						stream << msg[i];
					}
				}
				stream << "\n";
				return stream.str();
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