// © 2025 Adam Badke. All rights reserved.
#include "PerformanceTimer.h"
#include "PerformanceTimer_Platform.h"


namespace platform
{
	void (*PerformanceTimer::Create)(host::PerformanceTimer&) = nullptr;
	void (*PerformanceTimer::Start)(host::PerformanceTimer&) = nullptr;
	double (*PerformanceTimer::PeekMs)(host::PerformanceTimer const&) = nullptr;
	double (*PerformanceTimer::PeekSec)(host::PerformanceTimer const&) = nullptr;
}