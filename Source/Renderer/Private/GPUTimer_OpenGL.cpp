// © 2025 Adam Badke. All rights reserved.
#include "Private/GPUTimer_OpenGL.h"

#include "Core/PerfLogger.h"

#include "Core/Util/CastUtils.h"


namespace opengl
{
	void GPUTimer::PlatObj::Destroy()
	{
		SEAssert(!m_directComputeQueryIDs.empty() && !m_copyQueryIDs.empty(),
			"Trying to destroy an empty list of query IDs");

		glDeleteQueries(util::CheckedCast<GLsizei>(m_directComputeQueryIDs.size()), m_directComputeQueryIDs.data());
		m_directComputeQueryIDs.clear();

		glDeleteQueries(util::CheckedCast<GLsizei>(m_copyQueryIDs.size()), m_copyQueryIDs.data());
		m_copyQueryIDs.clear();
	}


	void GPUTimer::Create(re::GPUTimer const& timer)
	{
		opengl::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<opengl::GPUTimer::PlatObj*>();

		const uint8_t totalQueriesPerTimer = platObj->m_numFramesInFlight * 2; // x2 for start/end timestamps
		const GLsizei totalQuerySlots = totalQueriesPerTimer * re::GPUTimer::k_maxGPUTimersPerFrame;

		platObj->m_directComputeQueryIDs.resize(totalQuerySlots, 0);
		glGenQueries(totalQuerySlots, platObj->m_directComputeQueryIDs.data());

		platObj->m_copyQueryIDs.resize(totalQuerySlots, 0);
		glGenQueries(totalQuerySlots, platObj->m_copyQueryIDs.data());
		
		// Associate our query names and objects, and name our objects (New query names are not associated with a query
		// object until glBeginQuery is called)
		for (size_t i = 0; i < totalQuerySlots; ++i)
		{
			// Direct/compute:
			glBeginQuery(GL_TIME_ELAPSED, platObj->m_directComputeQueryIDs[i]);
			glEndQuery(GL_TIME_ELAPSED);

			SEAssert(glIsQuery(platObj->m_directComputeQueryIDs[i]), "GPUTimer::Create failed to create OpenGL query object");

			glObjectLabel(GL_QUERY, platObj->m_directComputeQueryIDs[i], -1, 
				std::format("Direct/Compute:GPUTimer{}:{}Query", i, i % 2 == 0 ? "Start" : "End").c_str());

			// Copy:
			glBeginQuery(GL_TIME_ELAPSED, platObj->m_copyQueryIDs[i]);
			glEndQuery(GL_TIME_ELAPSED);

			SEAssert(glIsQuery(platObj->m_copyQueryIDs[i]), "GPUTimer::Create failed to create OpenGL query object");

			glObjectLabel(GL_QUERY, platObj->m_copyQueryIDs[i], -1,
				std::format("Copy:GPUTimer{}:{}Query", i, i % 2 == 0 ? "Start" : "End").c_str());
		}

		platObj->m_invGPUFrequency = 1.f / 1000000.0; // OpenGL reports time in nanoseconds (ns)
	}


	void GPUTimer::BeginFrame(re::GPUTimer const& timer)
	{
		// 
	}


	std::vector<uint64_t> GPUTimer::EndFrame(re::GPUTimer const& timer, re::GPUTimer::TimerType timerType)
	{
		opengl::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<opengl::GPUTimer::PlatObj*>();

		std::vector<GLuint>* queryIDs = nullptr;
		switch (timerType)
		{
		case re::GPUTimer::TimerType::DirectCompute:
		{
			queryIDs = &platObj->m_directComputeQueryIDs;
		}
		break;
		case re::GPUTimer::TimerType::Copy:
		{
			queryIDs = &platObj->m_copyQueryIDs;
		}
		break;
		default: SEAssertF("Invalid timer type");
		}

		const uint8_t frameIdx = platObj->m_currentFrameIdx;

		// Readback our oldest queries:
		const uint32_t totalTimes = re::GPUTimer::k_maxGPUTimersPerFrame * 2;
		std::vector<uint64_t> gpuTimes(totalTimes, 0);

		const uint8_t oldestFrameIdx = (frameIdx + 1) % platObj->m_numFramesInFlight;
		const uint32_t queryStartOffset = oldestFrameIdx * re::GPUTimer::k_maxGPUTimersPerFrame * 2;

		for (size_t curQueryIdx = 0; curQueryIdx < totalTimes; curQueryIdx += 2) // Iterate over start/end pairs
		{
			const size_t curQueryStartIdx = queryStartOffset + curQueryIdx;
			const size_t curQueryEndIdx = curQueryStartIdx + 1;

			// Note: We don't check/wait for query results as they were issued in the previous frame

			// Get the query results:
			glGetQueryObjectui64v(queryIDs->at(curQueryStartIdx), GL_QUERY_RESULT, &gpuTimes[curQueryIdx]);
			glGetQueryObjectui64v(queryIDs->at(curQueryEndIdx), GL_QUERY_RESULT, &gpuTimes[curQueryIdx + 1]);
		}

		return gpuTimes;
	}


	void GPUTimer::StartTimer(re::GPUTimer const& timer, re::GPUTimer::TimerType timerType, uint32_t startQueryIdx, void*)
	{
		opengl::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<opengl::GPUTimer::PlatObj*>();

		std::vector<GLuint>* queryIDs = nullptr;
		switch (timerType)
		{
		case re::GPUTimer::TimerType::DirectCompute:
		{
			queryIDs = &platObj->m_directComputeQueryIDs;
		}
		break;
		case re::GPUTimer::TimerType::Copy:
		{
			queryIDs = &platObj->m_copyQueryIDs;
		}
		break;
		default: SEAssertF("Invalid timer type");
		}

		glQueryCounter(queryIDs->at(startQueryIdx), GL_TIMESTAMP);
	}


	void GPUTimer::StopTimer(re::GPUTimer const& timer, re::GPUTimer::TimerType timerType, uint32_t endQueryIdx, void*)
	{
		opengl::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<opengl::GPUTimer::PlatObj*>();

		std::vector<GLuint>* queryIDs = nullptr;
		switch (timerType)
		{
		case re::GPUTimer::TimerType::DirectCompute:
		{
			queryIDs = &platObj->m_directComputeQueryIDs;
		}
		break;
		case re::GPUTimer::TimerType::Copy:
		{
			queryIDs = &platObj->m_copyQueryIDs;
		}
		break;
		default: SEAssertF("Invalid timer type");
		}

		glQueryCounter(queryIDs->at(endQueryIdx), GL_TIMESTAMP);
	}
}