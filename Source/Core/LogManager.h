// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Definitions/ConfigKeys.h"

#include "Util/TextUtils.h"


namespace
{
	struct ImGuiLogWindow;
}

namespace core
{
	class LogManager final
	{
	public:
		static LogManager* Get(); // Singleton functionality


	public:
		/* Public logging interface:
		----------------------------*/
		template<typename...Args>
		static void Log(char const* msg, Args&&... args);
		
		template<typename...Args>
		static void Log(wchar_t const* msg, Args&&... args);

		template<typename... Args>
		static void LogWarning(char const* msg, Args&&... args);

		template<typename... Args>
		static void LogWarning(wchar_t const* msg, Args&&... args);

		template<typename... Args>
		static void LogError(char const* msg, Args&&... args);

		template<typename... Args>
		static void LogError(wchar_t const* msg, Args&&... args);


	public:
		LogManager();

		LogManager(LogManager&&) noexcept = default;
		LogManager& operator=(LogManager&&) noexcept = default;
		~LogManager() = default;

		void Startup(bool isSystemConsoleWindowEnabled);
		void Shutdown();

		void ShowImGuiWindow(bool* show);

	private:
		void Run(bool isSystemConsoleWindowEnabled); // LogManager thread

	private:
		constexpr static uint32_t k_internalStagingBufferSize = 4096;


		void AddMessage(char const*);
		std::unique_ptr<ImGuiLogWindow> m_imGuiLogWindow; // Internally contains a mutex

		bool m_isRunning;

		std::queue<std::array<char, k_internalStagingBufferSize>> m_messages;
		std::mutex m_messagesMutex;
		std::condition_variable m_messagesCV;

		std::ofstream m_logOutputStream;


	private:
		enum class LogType : uint8_t
		{
			Log,
			Warning,
			Error
		};

		
	private:
		template<typename T, typename... Args>
		static void LogInternal(LogType, T const* msg, Args&&... args);

		template<typename T>
		static void InsertMessageAndVariadicArgs(T* buf, uint32_t bufferSize, T const* msg, ...);

		template<typename T>
		static void InsertLogPrefix(
			T const* alignPrefix,
			size_t alignPrefixLen,
			T const* tag, 
			size_t tagLen, 
			T* destBuffer);
	

	private: // No copying allowed
		LogManager(LogManager const&) = delete;
		void operator=(LogManager const&) = delete;
	};


	template<typename...Args>
	inline void LogManager::Log(char const* msg, Args&&... args)
	{
		LogInternal<char, Args...>(LogManager::LogType::Log, msg, std::forward<Args>(args)...);
	}


	template<typename...Args>
	inline void LogManager::Log(wchar_t const* msg, Args&&... args)
	{
		LogInternal<wchar_t, Args...>(LogManager::LogType::Log, msg, std::forward<Args>(args)...);
	}

	template<typename... Args>
	inline void LogManager::LogWarning(char const* msg, Args&&... args)
	{
		LogInternal<char, Args...>(LogManager::LogType::Warning, msg, std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline void LogManager::LogWarning(wchar_t const* msg, Args&&... args)
	{

		LogInternal<wchar_t, Args...>(LogManager::LogType::Warning, msg, std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline void LogManager::LogError(char const* msg, Args&&... args)
	{
		LogInternal<char, Args...>(LogManager::LogType::Error, msg, std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline void LogManager::LogError(wchar_t const* msg, Args&&... args)
	{
		LogInternal<wchar_t, Args...>(LogManager::LogType::Error, msg, std::forward<Args>(args)...);
	}


	template<typename T, typename... Args>
	inline void LogManager::LogInternal(LogType logType, T const* msg, Args&&... args)
	{
		// Select the appropriate tag prefix:
		T const* tagPrefix = nullptr;
		size_t tagPrefixLen = 0;
		switch (logType)
		{
		case LogType::Log:
		{
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				tagPrefix = logging::k_logWPrefix;
			}
			else
			{
				tagPrefix = logging::k_logPrefix;
			}
			tagPrefixLen = logging::k_logPrefixLen;
		}
		break;
		case LogType::Warning:
		{
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				tagPrefix = logging::k_warnWPrefix;
			}
			else
			{
				tagPrefix = logging::k_warnPrefix;
			}			
			tagPrefixLen = logging::k_warnPrefixLen;
		}
		break;
		case LogType::Error:
		{
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				tagPrefix = logging::k_errorWPrefix;
			}
			else
			{
				tagPrefix = logging::k_errorPrefix;
			}
			tagPrefixLen = logging::k_errorPrefixLen;
		}
		break;
		default: break;
		}

		std::array<T, k_internalStagingBufferSize> stagingBuffer;

		// Prepend log prefix formatting:
		size_t prependLength = 0;
		T const* messageStart = nullptr;
		if (msg[0] == '\n')
		{
			T const* formatPrefix = nullptr;
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				formatPrefix = logging::k_newlineWPrefix;
			}
			else
			{
				formatPrefix = logging::k_newlinePrefix;
			}

			prependLength = logging::k_newlinePrefixLen + tagPrefixLen;
			messageStart = msg + 1;

			InsertLogPrefix<T>(
				formatPrefix,
				logging::k_newlinePrefixLen, 
				tagPrefix, 
				tagPrefixLen,
				stagingBuffer.data());
		}
		else if (msg[0] == '\t')
		{
			T const* formatPrefix = nullptr;
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				formatPrefix = logging::k_tabWPrefix;
			}
			else
			{
				formatPrefix = logging::k_tabPrefix;
			}

			prependLength = logging::k_tabPrefixLen;
			messageStart = msg + 1;

			InsertLogPrefix<T>(
				formatPrefix,
				logging::k_tabPrefixLen,
				nullptr,
				0,
				stagingBuffer.data());
		}
		else
		{
			prependLength = tagPrefixLen;
			messageStart = msg;

			InsertLogPrefix<T>(
				nullptr,
				0,
				tagPrefix,
				tagPrefixLen,
				stagingBuffer.data());
		}

		// Append the expanded message after our prefix formatting:
		InsertMessageAndVariadicArgs<T>(
			stagingBuffer.data() + prependLength, 
			k_internalStagingBufferSize - static_cast<uint32_t>(prependLength + 2), // +2 for null terminator and new line
			messageStart,
			std::forward<Args>(args)...);

		// Finally, pass the message to our LogManager singleton:
		static LogManager* const s_logMgr = LogManager::Get();
		if constexpr (std::is_same<T, wchar_t>::value)
		{
			s_logMgr->AddMessage(util::FromWideCString(stagingBuffer.data()).c_str());
		}
		else
		{
			s_logMgr->AddMessage(stagingBuffer.data());
		}
	}


	template<typename T>
	void LogManager::InsertLogPrefix(
		T const* alignPrefix,
		size_t alignPrefixLen,
		T const* tag,
		size_t tagLen,
		T* destBuffer)
	{
#if defined(_DEBUG)
		assert((alignPrefix || alignPrefixLen == 0) && (tag || tagLen == 0));
#endif

		if (alignPrefix)
		{
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				wcsncpy(destBuffer, alignPrefix, alignPrefixLen + 1); // +1 to also copy the null terminator
			}
			else
			{
				strncpy(destBuffer, alignPrefix, alignPrefixLen + 1); // +1 to also copy the null terminator
			}			
		}
		if (tag)
		{
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				wcsncpy(destBuffer + alignPrefixLen, tag, tagLen + 1); // +1 to also copy the null terminator
			}
			else
			{
				strncpy(destBuffer + alignPrefixLen, tag, tagLen + 1); // +1 to also copy the null terminator
			}
		}

		const size_t prependLenth = alignPrefixLen + tagLen;

		if constexpr (std::is_same<T, wchar_t>::value)
		{
			wcsncpy(destBuffer + prependLenth, L"\n\0", 2); // newline and null terminator, incase the message is empty
		}
		else
		{
			strncpy(destBuffer + prependLenth, "\n\0", 2); // newline and null terminator, incase the message is empty
		}
	}


	template<typename T>
	void LogManager::InsertMessageAndVariadicArgs(T* buf, uint32_t bufferSize, T const* msg, ...)
	{
		va_list args;
		va_start(args, msg);

		int numChars = 0;
		if constexpr (std::is_same<T, wchar_t>::value)
		{
			numChars = vswprintf_s(buf, bufferSize, msg, args);
		}
		else
		{
			numChars = vsprintf_s(buf, bufferSize, msg, args);
		}
#if defined(_DEBUG)
		assert(static_cast<uint32_t>(numChars) < bufferSize && 
			"Message is larger than the buffer size; it will be truncated");
#endif

		va_end(args);

		if constexpr (std::is_same<T, wchar_t>::value)
		{
			wcsncpy(buf + numChars, L"\n\0", 2); // Terminate with newline and a null char
		}
		else
		{
			strncpy(buf + numChars, "\n\0", 2); // Terminate with newline and a null char
		}		
	}
}


// Log macros:
// ------------------------------------------------
#define LOG(msg, ...)			core::LogManager::Log(msg, __VA_ARGS__);
#define LOG_WARNING(msg, ...)	core::LogManager::LogWarning(msg, __VA_ARGS__);
#define LOG_ERROR(msg, ...)		core::LogManager::LogError(msg, __VA_ARGS__);