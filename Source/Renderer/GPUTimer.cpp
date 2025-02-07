// © 2025 Adam Badke. All rights reserved.
#include "GPUTimer.h"
#include "GPUTimer_Platform.h"

#include "Core/Assert.h"
#include "Core/PerfLogger.h"

#include "Core/Util/CastUtils.h"


namespace re
{
	GPUTimer::Handle::Handle()
		: m_timerRecordItr{}
		, m_gpuTimer(nullptr)
	{
	}


	GPUTimer::Handle::Handle(
		GPUTimer* gpuTimer, std::multimap<util::HashKey, TimeRecord>::iterator timeRecordItr)
		: m_timerRecordItr(timeRecordItr)
		, m_gpuTimer(gpuTimer)
	{
	}


	GPUTimer::Handle::Handle(Handle&& rhs) noexcept
	{
		*this = std::move(rhs);
	}


	GPUTimer::Handle& GPUTimer::Handle::operator=(Handle&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_timerRecordItr = rhs.m_timerRecordItr;
			rhs.m_timerRecordItr = {};

			m_gpuTimer = rhs.m_gpuTimer;
			rhs.m_gpuTimer = nullptr;
		}
		return *this;
	}


	GPUTimer::Handle::~Handle()
	{
		SEAssert(m_gpuTimer == nullptr, "GPU Timer Handle being destroyed before StopTimer() was called");
	}


	void GPUTimer::Handle::StopTimer(void* platformObject)
	{
		// To simplify our interface, it is valid to stop a timer that was never started
		if (m_gpuTimer)
		{
			m_gpuTimer->StopTimer(m_timerRecordItr, platformObject);
			m_gpuTimer = nullptr;
		}
	}


	// -----------------------------------------------------------------------------------------------------------------


	GPUTimer::GPUTimer(core::PerfLogger* perfLogger, uint8_t numFramesInFlight)
		: m_platformParams(platform::GPUTimer::CreatePlatformParams())
		, m_perfLogger(perfLogger)
	{
		SEAssert(m_perfLogger && numFramesInFlight > 0 && numFramesInFlight <= 3, "Invalid args received");

		m_platformParams->m_numFramesInFlight = numFramesInFlight;		
		m_platformParams->m_currentFrameNum = 0;
		m_platformParams->m_currentFrameTimerCount = 0;
	}


	GPUTimer::~GPUTimer()
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			SEAssert(m_platformParams == nullptr, "Platform params is not null. Was Destroy() called?");
		}
	}
	

	void GPUTimer::Create(void const* createParams)
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			platform::GPUTimer::Create(*this, createParams);

			m_platformParams->m_isCreated = true;
		}
	}


	void GPUTimer::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			SEAssert(m_platformParams && m_platformParams->m_isCreated, "GPU Timer already Destroyed");

			platform::GPUTimer::Destroy(*this);
			m_platformParams = nullptr;
		}
	}


	void GPUTimer::BeginFrame(uint64_t frameNum)
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			SEAssert(m_platformParams->m_isCreated, "GPU timer has not been created. Was Create() called?");

			m_platformParams->m_currentFrameNum = frameNum;
			m_platformParams->m_currentFrameTimerCount = 0;
			platform::GPUTimer::BeginFrame(*this);
		}
	}


	void GPUTimer::EndFrame(void* platformObject)
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			SEAssert(m_platformParams->m_isCreated, "GPU timer has not been created. Was Create() called?");

			std::vector<uint64_t> const& readbackTimes = platform::GPUTimer::EndFrame(*this, platformObject);
			SEAssert(!readbackTimes.empty(), "Failed to retrieve GPU times");

			// Clear any GPU timers that have not been updated in a while
			for (auto recordItr = m_platformParams->m_gpuTimes.begin(); recordItr != m_platformParams->m_gpuTimes.end(); )
			{
				recordItr->second.m_numFramesSinceUpdated++;
				if (recordItr->second.m_numFramesSinceUpdated > GPUTimer::k_maxFramesWithoutUpdate)
				{
					recordItr = m_platformParams->m_gpuTimes.erase(recordItr);
				}
				else
				{
					++recordItr;
				}
			}

			// Update the PerfLogger with the (oldest) frame results:
			const uint8_t oldestFrameIdx = 
				static_cast<uint8_t>((m_platformParams->m_currentFrameNum + 1) % m_platformParams->m_numFramesInFlight);

			std::unordered_set<util::HashKey> seenKeys;
			for (auto& multiRecord : m_platformParams->m_gpuTimes)
			{
				if (seenKeys.emplace(multiRecord.first).second)
				{
					double totalTime = 0.0;
					
					auto recordRange = m_platformParams->m_gpuTimes.equal_range(multiRecord.first);
					auto recordItr = recordRange.first;
					while (recordItr != recordRange.second)
					{
						TimeRecord& timeRecord = recordItr->second;
						const uint32_t startIdx = timeRecord.m_queryIndexes[oldestFrameIdx];
						if (startIdx != re::GPUTimer::k_invalidQueryIdx)
						{
							const uint32_t endIdx = startIdx + 1;
							SEAssert(endIdx < GPUTimer::k_maxGPUTimersPerFrame * m_platformParams->m_numFramesInFlight * 2,
								"OOB index");

							totalTime += readbackTimes[endIdx] - readbackTimes[startIdx];

							// Reset to our "no timer recorded for this frame" sentinel
							timeRecord.m_queryIndexes[oldestFrameIdx] = re::GPUTimer::k_invalidQueryIdx;
						}

						++recordItr;
					}

					m_perfLogger->NotifyPeriod(
						totalTime * m_platformParams->m_invGPUFrequency,
						recordRange.first->second.m_name.c_str(),
						recordRange.first->second.m_parentName.empty() ? 
							nullptr : recordRange.first->second.m_parentName.c_str());
				}
			}
		}
	}


	GPUTimer::Handle GPUTimer::StartTimer(
		void* platformObject, char const* name, char const* parentName /*= nullptr*/)
	{
		Handle newHandle(this, StartHandleTimer(platformObject, name, parentName));

		return newHandle;
	}


	std::multimap<util::HashKey, GPUTimer::TimeRecord>::iterator GPUTimer::StartHandleTimer(
		void* platformObject,
		char const* name,
		char const* parentName /*= nullptr*/)
	{
		const util::HashKey nameHash(name);

		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			const uint8_t frameIdx = (m_platformParams->m_currentFrameNum % m_platformParams->m_numFramesInFlight);
			const uint32_t firstFrameQueryIdx = frameIdx * GPUTimer::k_maxGPUTimersPerFrame * 2;

			auto recordRange = m_platformParams->m_gpuTimes.equal_range(nameHash);
			auto recordItr = recordRange.first;
			while (recordItr != recordRange.second)
			{
				if (recordItr->second.m_queryIndexes[frameIdx] == re::GPUTimer::k_invalidQueryIdx)
				{
					recordItr->second.m_numFramesSinceUpdated = 0; // Found an empty slot, reuse it!
					break;
				}
				++recordItr;
			}

			if (recordItr == recordRange.second) // No empty query slot found: Create a new record
			{
				recordItr = m_platformParams->m_gpuTimes.emplace(
					nameHash,
					TimeRecord{
						.m_name = name,
						.m_parentName = parentName ? parentName : std::string(/*empty*/),
						.m_numFramesSinceUpdated = 0,
					});

				for (auto& queryIdx : recordItr->second.m_queryIndexes)
				{
					queryIdx = re::GPUTimer::k_invalidQueryIdx;
				}
			}

			SEAssert(m_platformParams->m_currentFrameTimerCount < (re::GPUTimer::k_maxGPUTimersPerFrame - 1),
				"About to request more timers than are available. Consider increasing k_maxGPUTimersPerFrame");

			const uint32_t relativeQueryIdx = (m_platformParams->m_currentFrameTimerCount++) * 2; // x2 for start/end timestamps
			const uint32_t startQueryIdx = firstFrameQueryIdx + relativeQueryIdx;

			platform::GPUTimer::StartTimer(*this, startQueryIdx, platformObject);

			recordItr->second.m_queryIndexes[frameIdx] = relativeQueryIdx;

			return recordItr;
		}
	}

	void GPUTimer::StopTimer(std::multimap<util::HashKey, TimeRecord>::iterator timeRecordItr, void* platformObject)
	{
		{
			std::lock_guard<std::mutex> lock(m_platformParamsMutex);

			const uint8_t frameIdx = (m_platformParams->m_currentFrameNum % m_platformParams->m_numFramesInFlight);

			TimeRecord& timeRecord = timeRecordItr->second;
			if (timeRecord.m_queryIndexes[frameIdx] != re::GPUTimer::k_invalidQueryIdx)
			{
				const uint32_t firstFrameQueryIdx = frameIdx * GPUTimer::k_maxGPUTimersPerFrame * 2;

				const uint32_t relativeStartQueryIdx = timeRecord.m_queryIndexes[frameIdx];
				const uint32_t endQueryIdx = firstFrameQueryIdx + relativeStartQueryIdx + 1; // +1 for end query

				platform::GPUTimer::StopTimer(*this, endQueryIdx, platformObject);
			}
		}
	}
}