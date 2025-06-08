// © 2024 Adam Badke. All rights reserved.
#include "Private/Assert.h"
#include "Private/Logger.h"


namespace assertinternal
{
	void HandleAssertInternal(char const* msg)
	{
		LOG_ERROR(msg);

		// TODO: Move this Win32-specific stuff to a platform wrapper
		::ClipCursor(nullptr);
		::SetCursor(::LoadCursor(NULL, IDC_ARROW)); // Restore the default arrow icon cursor
	}


	void LogAssertAsError(char const* msg)
	{
		LOG_ERROR(msg);
	}


	std::string StringFromVariadicArgs(char const* msg, ...)
	{
		constexpr size_t k_bufferSize = 4096;
		std::array<char, 4096> buf{ '\0' };

		va_list args;
		va_start(args, msg);

		int numChars = vsprintf_s(buf.data(), k_bufferSize, msg, args);

#if defined(_DEBUG)
		assert(static_cast<uint32_t>(numChars) < k_bufferSize &&
			"Message is larger than the buffer size; it will be truncated");
#endif

		va_end(args);

		strncpy(buf.data() + numChars, "\0", 1); // Terminate with a null char

		return std::string(buf.data());
	}
}