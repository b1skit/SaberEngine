// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Private/Host/PerformanceTimer.h"

#include "Private/Interfaces/IEventListener.h"

#include "Private/Util/HashKey.h"


namespace core
{
	class PerfLogger final : public virtual core::IEventListener
	{
	public:
		static PerfLogger* Get(); // Singleton functionality


	public:
		PerfLogger();

		PerfLogger(PerfLogger&&) noexcept = default;
		PerfLogger& operator=(PerfLogger&&) noexcept = default;
		
		~PerfLogger();

	
	public:
		void BeginFrame();

		void NotifyBegin(char const* name, char const* parentName = nullptr);
		void NotifyEnd(char const* name);

		void NotifyPeriod(double totalTimeMs, char const*, char const* parentName = nullptr);


	public:
		void ShowImGuiWindow(bool* show);


	private:
		void HandleEvents() override;

		void Destroy();


	private:
		static constexpr uint8_t k_maxFramesWithoutUpdate = 10; // No. frames without update to delete a record
		
		struct TimeRecord
		{
			host::PerformanceTimer m_timer;

			std::string m_name;
			util::HashKey m_nameHash;

			std::string m_parentName;
			util::HashKey m_parentNameHash;

			double m_mostRecentTimeMs = 0.0;

			std::vector<util::HashKey> m_children;
			bool m_hasParent = false; // If true, will be nested

			uint8_t m_numFramesSinceUpdated = 0;
		};
		std::unordered_map<util::HashKey, TimeRecord> m_times;

		static constexpr double k_warnThresholdMs = 1000.0 / 70.0;
		static constexpr double k_alertThresholdMs = 1000.0 / 60.0;

		uint8_t m_numFramesInFlight;

		std::mutex m_perfLoggerMutex;
		
		std::atomic<bool> m_isEnabled;


	private:
		TimeRecord& AddUpdateTimeRecordHelper(char const* name, char const* parentName /*= nullptr*/);


	private: // No copying allowed
		PerfLogger(PerfLogger const&) = delete;
		void operator=(PerfLogger const&) = delete;
	};
}