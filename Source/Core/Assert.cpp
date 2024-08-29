// © 2024 Adam Badke. All rights reserved.
#include "Assert.h"
#include "LogManager.h"


void HandleLogError(char const* msg)
{
	LOG_ERROR(msg);
}


#if defined(_DEBUG)

void HandleAssertInternal()
{
	::ClipCursor(nullptr);
	::SetCursor(::LoadCursor(NULL, IDC_ARROW)); // Restore the default arrow icon cursor
}

#endif