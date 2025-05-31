// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IEventListener.h"
#include "Core/Interfaces/IPlatformObject.h"

#include "Core/Util/HashKey.h"


namespace core
{
	class PerfLogger;
}

namespace re
{
	class GPUTimer : public virtual core::IEventListener
	{
	public:
		// Arbitrary: How many timers should we allocate? Each timer allocates 2 query elements (i.e. start + stop)
		static constexpr uint32_t k_maxGPUTimersPerFrame = 512;
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

		struct PlatObj : public core::IPlatObj
		{
			virtual void Destroy() override = 0;

			std::multimap<util::HashKey, TimeRecord> m_directComputeTimes;
			std::multimap<util::HashKey, TimeRecord> m_copyTimes;

			double m_invGPUFrequency = 0.0; // 1.0 / (Ticks/ms)

			uint64_t m_currentFrameNum = 0;
			uint8_t m_currentFrameIdx = 0;
			uint8_t m_numFramesInFlight = 0;			
			
			uint32_t m_currentDirectComputeTimerCount = 0; // How many direct/compute queue timers started this frame?
			uint32_t m_currentCopyTimerCount = 0; // How many copy queue timers started this frame?			

			bool m_isCreated = false;
		};


	public:
		enum class TimerType : uint8_t
		{
			DirectCompute,
			Copy,
			
			Invalid,
		};


		class Handle
		{
		public:
			Handle();
			Handle(GPUTimer*, TimerType, std::multimap<util::HashKey, TimeRecord>::iterator);

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
			TimerType m_type;
		};


	public:
		GPUTimer(core::PerfLogger*, uint8_t numFramesInFlight);

		GPUTimer(GPUTimer&&) noexcept = default;
		GPUTimer& operator=(GPUTimer&&) noexcept = default;

		~GPUTimer();

		void Destroy();

		PlatObj* GetPlatformObject() const;


	public: // Note: Use platformObject to pass external dependencies (e.g. DX12 graphics command lists)
		void BeginFrame(uint64_t frameNum);
		void EndFrame();

		[[nodiscard]] Handle StartTimer(void* platformObject, char const* name, char const* parentName = nullptr);
		[[nodiscard]] Handle StartCopyTimer(void* platformObject, char const* name, char const* parentName = nullptr);


	private:
		void HandleEvents() override;

		void Create();		


	private:
		friend class Handle;
		std::multimap<util::HashKey, TimeRecord>::iterator StartHandleTimer(
			TimerType, void* platformObject, char const* name, char const* parentName = nullptr);

		void StopTimer(TimerType, std::multimap<util::HashKey, TimeRecord>::iterator, void* platformObject);


	private:
		std::unique_ptr<PlatObj> m_platObj;
		mutable std::mutex m_platformParamsMutex;

		core::PerfLogger* m_perfLogger;
		
		std::atomic<bool> m_isEnabled;


	private: // No copying allowed
		GPUTimer() = delete;
		GPUTimer& operator=(GPUTimer const&) = delete;
		GPUTimer(GPUTimer const&) = delete;
	};


	inline GPUTimer::PlatObj* GPUTimer::GetPlatformObject() const
	{
		return m_platObj.get();
	}
}