// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "Definitions/ConfigKeys.h"

#include "Util/TextUtils.h"


namespace
{
	struct ImGuiLogWindow;
}

namespace core
{
	class Logger final
	{
	public:
		/* Public logging interface:
		----------------------------*/
		template<typename...Args>
		static void Log(std::string_view msg, Args&&... args);
		
		template<typename...Args>
		static void Log(std::wstring_view msg, Args&&... args);

		template<typename... Args>
		static void LogWarning(std::string_view msg, Args&&... args);

		template<typename... Args>
		static void LogWarning(std::wstring_view msg, Args&&... args);

		template<typename... Args>
		static void LogError(std::string_view msg, Args&&... args);

		template<typename... Args>
		static void LogError(std::wstring_view msg, Args&&... args);


	public:
		static void Startup(bool isSystemConsoleWindowEnabled);
		static void Shutdown();

		static void ShowImGuiWindow(bool* show);

	private:
		static void Run(); // Logger thread

	private:
		constexpr static uint32_t k_internalStagingBufferSize = 4096;


		static void AddMessage(char const*);
		static std::unique_ptr<ImGuiLogWindow> s_imGuiLogWindow; // Internally contains a mutex

		static bool s_isRunning;
		static bool s_showHostConsole;

		static std::queue<std::array<char, k_internalStagingBufferSize>> s_messages;
		static std::mutex s_messagesMutex;
		static std::condition_variable s_messagesCV;

		static std::ofstream s_logOutputStream;


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
			size_t aliLoggnPrefixLen,
			T const* tag, 
			size_t tagLen, 
			T* destBuffer);
	

	private: // Static class only
		Logger() = delete;
		Logger(Logger const&) = delete;
		Logger(Logger&&) noexcept = delete;
		Logger& operator=(Logger&&) noexcept = delete;
		void operator=(Logger const&) = delete;
		~Logger() = default;
	};


	template<typename...Args>
	inline void Logger::Log(std::string_view msg, Args&&... args)
	{
		SEAssert(msg.data()[msg.size()] == '\0', "std::string_view must be null-terminated for Logger usage");
		LogInternal<char, Args...>(Logger::LogType::Log, msg.data(), std::forward<Args>(args)...);
	}


	template<typename...Args>
	inline void Logger::Log(std::wstring_view msg, Args&&... args)
	{
		SEAssert(msg.data()[msg.size()] == L'\0', "std::wstring_view must be null-terminated for Logger usage");
		LogInternal<wchar_t, Args...>(Logger::LogType::Log, msg.data(), std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline void Logger::LogWarning(std::string_view msg, Args&&... args)
	{
		SEAssert(msg.data()[msg.size()] == '\0', "std::string_view must be null-terminated for Logger usage");
		LogInternal<char, Args...>(Logger::LogType::Warning, msg.data(), std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline void Logger::LogWarning(std::wstring_view msg, Args&&... args)
	{
		SEAssert(msg.data()[msg.size()] == L'\0', "std::wstring_view must be null-terminated for Logger usage");
		LogInternal<wchar_t, Args...>(Logger::LogType::Warning, msg.data(), std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline void Logger::LogError(std::string_view msg, Args&&... args)
	{
		SEAssert(msg.data()[msg.size()] == '\0', "std::string_view must be null-terminated for Logger usage");
		LogInternal<char, Args...>(Logger::LogType::Error, msg.data(), std::forward<Args>(args)...);
	}


	template<typename... Args>
	inline void Logger::LogError(std::wstring_view msg, Args&&... args)
	{
		SEAssert(msg.data()[msg.size()] == L'\0', "std::wstring_view must be null-terminated for Logger usage");
		LogInternal<wchar_t, Args...>(Logger::LogType::Error, msg.data(), std::forward<Args>(args)...);
	}


	template<typename T, typename... Args>
	inline void Logger::LogInternal(LogType logType, T const* msg, Args&&... args)
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

		// Finally, pass the message to our Logger singleton:
		if constexpr (std::is_same<T, wchar_t>::value)
		{
			AddMessage(util::FromWideCString(stagingBuffer.data()).c_str());
		}
		else
		{
			AddMessage(stagingBuffer.data());
		}
	}


	template<typename T>
	void Logger::InsertLogPrefix(
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
	void Logger::InsertMessageAndVariadicArgs(T* buf, uint32_t bufferSize, T const* msg, ...)
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
#define LOG(msg, ...)			core::Logger::Log(msg, __VA_ARGS__);
#define LOG_WARNING(msg, ...)	core::Logger::LogWarning(msg, __VA_ARGS__);
#define LOG_ERROR(msg, ...)		core::Logger::LogError(msg, __VA_ARGS__);