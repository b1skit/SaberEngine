#pragma once

#include <string>
#include <sstream>
#include <iostream>
using std::cout;

#include "EventListener.h"
#include "EngineComponent.h"


namespace fr
{
	class LogManager : public virtual SaberEngine::EngineComponent, public SaberEngine::EventListener
	{
	public:
		LogManager() : EngineComponent("LogManager") {}
		~LogManager() = default;

		// Singleton functionality:
		static LogManager& Instance();

		// Disallow copying of our Singleton
		LogManager(LogManager const&) = delete; 
		LogManager(LogManager&&) = delete;
		void operator=(LogManager const&) = delete;

		// EngineComponent interface:
		void Startup();
		void Shutdown();
		void Update();

		// EventListener interface:
		void HandleEvent(SaberEngine::EventInfo const* eventInfo);

	private:


	public:
		// Templated static functions:
		//----------------------------
		template<typename... Args>
		static void Log(Args&& ... args)
		{
			std::ostringstream stream;
			(stream << ... << std::forward<Args>(args));

			if (stream.str()[0] == '\n')
			{
				cout << "\nLog:\t" << stream.str().substr(1, string::npos) << "\n";
			}
			else if (stream.str()[0] == '\t')
			{
				cout << "\t" << stream.str().substr(1, string::npos) << "\n";
			}
			else
			{
				cout << "Log:\t" << stream.str() << "\n";
			}
		}


		template<typename... Args>
		static void LogWarning(Args&& ... args)
		{
			std::ostringstream stream;
			(stream << ... << std::forward<Args>(args));

			if (stream.str()[0] == '\n')
			{
				cout << "\nWarn:\t" << stream.str().substr(1, string::npos) << "\n";
			}
			else if (stream.str()[0] == '\t')
			{
				cout << "\t" << stream.str().substr(1, string::npos) << "\n";
			}
			else
			{
				cout << "Warn:\t" << stream.str() << "\n";
			}
		}


		template<typename... Args>
		static void LogError(Args&& ... args)
		{
			std::ostringstream stream;
			(stream << ... << std::forward<Args>(args));

			if (stream.str()[0] == '\n')
			{
				cout << "\nError:\t" << stream.str().substr(1, string::npos) << "\n";
			}
			else if (stream.str()[0] == '\t')
			{
				cout << "\t" << stream.str().substr(1, string::npos) << "\n";
			}
			else
			{
				cout << "Error:\t" << stream.str() << "\n";
			}
		}		
	};
}


// Log Manager macros:
// ------------------------------------------------

#if defined (_DEBUG)
	#include "LogManager.h"
	#define LOG(...)			fr::LogManager::Log(__VA_ARGS__);
	#define LOG_WARNING(...)	fr::LogManager::LogWarning(__VA_ARGS__);
	#define LOG_ERROR(...)		fr::LogManager::LogError(__VA_ARGS__);
#else
	// Disable compiler warning C4834: discarding return value of function with 'nodiscard' attribute
	#pragma warning(disable : 4834) 
	#define LOG(...)			do {__VA_ARGS__;} while(false);
	#define LOG_WARNING(...)	do {__VA_ARGS__;} while(false);
	#define LOG_ERROR(...)		do {__VA_ARGS__;} while(false);
	// __VA_ARGS__; marks our arguments as "used" to the compiler, to avoid warning C4101: unreferenced local variable 
#endif