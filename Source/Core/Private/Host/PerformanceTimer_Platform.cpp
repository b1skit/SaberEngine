// © 2025 Adam Badke. All rights reserved.
#include "Private/PerformanceTimer.h"
#include "Private/PerformanceTimer_Platform.h"


namespace platform
{
	void (*PerformanceTimer::Create)(host::PerformanceTimer&) = nullptr;
	void (*PerformanceTimer::Start)(host::PerformanceTimer&) = nullptr;
	double (*PerformanceTimer::PeekMs)(host::PerformanceTimer const&) = nullptr;
	double (*PerformanceTimer::PeekSec)(host::PerformanceTimer const&) = nullptr;
}