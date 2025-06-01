// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace host
{
	class PerformanceTimer;
}

namespace platform
{
	class PerformanceTimer final
	{
	public:
		static void (*Create)(host::PerformanceTimer&);
		static void (*Start)(host::PerformanceTimer&);
		static double (*PeekMs)(host::PerformanceTimer const&);
		static double (*PeekSec)(host::PerformanceTimer const&);
	};
}