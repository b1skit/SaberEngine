// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace platform
{
	class SysInfo
	{
	public:
		static uint8_t(*GetMaxRenderTargets)();
	};
}