// � 2023 Adam Badke. All rights reserved.
#include "SysInfo_Platform.h"


namespace platform
{
	uint8_t (*SysInfo::GetMaxRenderTargets)() = nullptr;
}