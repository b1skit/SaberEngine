// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IPlatformParams.h"

#include "Core/Util/HashKey.h"


namespace core
{
	class PerfLogger;
}

namespace re
{
	class GPUTimer
	{
	public:
		// Arbitrary: How many timers should we allocate? Each timer allocates 2 query elements (i.e. start + stop)
		static constexpr uint32_t k_maxGPUTimersPerFrame = 256;
		static constexpr uint32_t k_invalidQueryIdx = std::numeric_limits<uint32_t>::max(); // No timer record

		// No. frames without update before deleting a record. Large to ensure nothing is freed while still in use
		static constexpr uint8_t k_maxFramesWithoutUpdate = 10;


	public:
		struct TimeRecord
		{
			// IDs are a relative per-frame query indices. We allocate 3 elements but only use what is required
			std::array<uint32_t, 3> m_queryIndexes;

			std::string m_name;
			std::string m_parentName;

			uint8_t m_numFramesSinceUpdated = 0;
		};

		struct PlatformParams : public core::IPlatformParams
		{
			std::multimap<util::HashKey, TimeRecord> m_gpuTimes;

			double m_invGPUFrequency = 0.0; // 1.0 / (Ticks/ms)

			uint64_t m_currentFrameNum = 0;
			uint8_t m_numFramesInFlight = 0;			
			uint8_t m_currentFrameTimerCount = 0; // How many timers started this frame?

			bool m_isCreated = false;
		};


	public:
		class Handle
		{
		public:
			Handle();
			Handle(GPUTimer*, std::multimap<util::HashKey, TimeRecord>::iterator);

			Handle(Handle&&) noexcept;
			Handle& operator=(Handle&&) noexcept;

			Handle(Handle const&) = default;
			Handle& operator=(Handle const&) = default;

			~Handle();
		
		public:
			void StopTimer(void* platformObject);


		private:
			std::multimap<util::HashKey, TimeRecord>::iterator m_timerRecordItr;
			GPUTimer* m_gpuTimer;
		};


	public:
		GPUTimer(core::PerfLogger*, uint8_t numFramesInFlight);

		GPUTimer(GPUTimer&&) noexcept = default;
		GPUTimer& operator=(GPUTimer&&) noexcept = default;

		~GPUTimer();

		void Create(void const* createParams);
		void Destroy();

		PlatformParams* GetPlatformParams() const;


	public: // Note: Use platformObject to pass external dependencies (e.g. DX12 graphics command lists)
		void BeginFrame(uint64_t frameNum);
		void EndFrame(void* platformObject);

		[[nodiscard]] Handle StartTimer(void* platformObject, char const* name, char const* parentName = nullptr);


	private:
		friend class Handle;
		std::multimap<util::HashKey, TimeRecord>::iterator StartHandleTimer(
			void* platformObject, char const* name, char const* parentName = nullptr);
		
		void StopTimer(std::multimap<util::HashKey, TimeRecord>::iterator, void* platformObject);

	private:
		std::unique_ptr<PlatformParams> m_platformParams;
		std::mutex m_platformParamsMutex;

		core::PerfLogger* m_perfLogger;


	private: // No copying allowed
		GPUTimer() = delete;
		GPUTimer& operator=(GPUTimer const&) = delete;
		GPUTimer(GPUTimer const&) = delete;
	};


	inline GPUTimer::PlatformParams* GPUTimer::GetPlatformParams() const
	{
		return m_platformParams.get();
	}
}