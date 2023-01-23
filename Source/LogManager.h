// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "EventListener.h"
#include "EngineComponent.h"


namespace
{
	struct ImGuiLogWindow;
}

namespace en
{
	class LogManager final : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		static LogManager* Get(); // Singleton functionality


	public:
		/* Public logging interface:
		----------------------------*/
		template<typename...Args>
		inline static void Log(char const* msg, Args&&... args);
		
		template<typename...Args>
		inline static void Log(wchar_t const* msg, Args&&... args);

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
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// EventListener interface:
		void HandleEvents() override;


	private:
		void AddMessage(std::string&& msg);
		std::unique_ptr<ImGuiLogWindow> m_imGuiLogWindow; // Internally contains a mutex


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
		
		template<typename... Args>
		inline static void LogInternal(wchar_t const* tagPrefix, wchar_t const* msg, Args&&... args);

		static void AssembleStringFromVariadicArgs(char* buf, uint32_t bufferSize, char const* fmt, ...);
		static void AssembleStringFromVariadicArgs(wchar_t* buf, uint32_t bufferSize, wchar_t const* fmt, ...);

		static std::string FormatStringForLog(char const* prefix, char const* tag, char const* assembledMsg);
		static std::wstring FormatStringForLog(wchar_t const* prefix, wchar_t const* tag, wchar_t const* assembledMsg);
	};


	template<typename...Args>
	inline static void LogManager::Log(char const* msg, Args&&... args)
	{
		LogInternal("Log:\t", msg, std::forward<Args>(args)...);
	}


	template<typename...Args>
	inline static void LogManager::Log(wchar_t const* msg, Args&&... args)
	{
		LogInternal(L"Log:\t", msg, std::forward<Args>(args)...);
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
		constexpr uint32_t bufferSize = 256;
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



	template<typename... Args>
	inline static void LogManager::LogInternal(wchar_t const* tagPrefix, wchar_t const* msg, Args&&... args)
	{
		constexpr uint32_t bufferSize = 256;
		std::array<wchar_t, bufferSize> assembledMsg;

		AssembleStringFromVariadicArgs(assembledMsg.data(), bufferSize, msg, args...);

		std::wstring formattedStr;
		if (msg[0] == '\n')
		{
			formattedStr = FormatStringForLog(L"\n", tagPrefix, &assembledMsg.data()[1]);
		}
		else if (msg[0] == '\t')
		{
			formattedStr = FormatStringForLog(L"\t", nullptr, &assembledMsg.data()[1]);
		}
		else
		{
			formattedStr = FormatStringForLog(nullptr, tagPrefix, assembledMsg.data());
		}

		// Convert wstring -> string. Note: This is deprecated. TODO: Handle this correctly
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wstringConverter;
		std::string convertedStr = wstringConverter.to_bytes(formattedStr);

		LogManager::Get()->AddMessage(std::move(convertedStr));
	}
}


// Log macros:
// ------------------------------------------------
#define LOG(msg, ...)			en::LogManager::Log(msg, __VA_ARGS__);
#define LOG_WARNING(msg, ...)	en::LogManager::LogWarning(msg, __VA_ARGS__);
#define LOG_ERROR(msg, ...)		en::LogManager::LogError(msg, __VA_ARGS__);