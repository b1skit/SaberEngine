// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Host/PerformanceTimer.h"

#include "Util/CHashKey.h"


namespace core
{
	class PerfLogger
	{
	public:
		static PerfLogger* Get(); // Singleton functionality


	public:
		PerfLogger() = default;
		PerfLogger(PerfLogger&&) noexcept = default;
		PerfLogger& operator=(PerfLogger&&) noexcept = default;
		
		~PerfLogger();

	
	public:
		void Register(util::CHashKey key, double warnThresholdMs = 14.0, double alertThresholdMs = 16.0);
		
		void NotifyBegin(util::CHashKey key);
		void NotifyEnd(util::CHashKey key);

		void NotifyPeriod(util::CHashKey key, double totalTimeMs);


	public:
		void ShowImGuiWindow(bool* show);


	private:
		struct TimeRecord
		{
			host::PerformanceTimer m_timer;

			double m_mostRecentTimeMs = 0.0;
			
			double m_warnThresholdMs = std::numeric_limits<double>::max();
			double m_alertThresholdMs = std::numeric_limits<double>::max();
		};
		std::unordered_map<util::CHashKey, TimeRecord> m_times;
		std::shared_mutex m_timesMutex;


	private: // No copying allowed
		PerfLogger(PerfLogger const&) = delete;
		void operator=(PerfLogger const&) = delete;
	};
}