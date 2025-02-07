// © 2025 Adam Badke. All rights reserved.
#include "GPUTimer_OpenGL.h"

#include "Core/PerfLogger.h"

#include "Core/Util/CastUtils.h"


namespace opengl
{
	void GPUTimer::PlatformParams::Destroy()
	{
		SEAssert(!m_queryIDs.empty(), "Trying to destroy an empty list of query IDs");

		glDeleteQueries(util::CheckedCast<GLsizei>(m_queryIDs.size()), m_queryIDs.data());
		m_queryIDs.clear();
	}


	void GPUTimer::Create(re::GPUTimer const& timer)
	{
		opengl::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<opengl::GPUTimer::PlatformParams*>();

		const uint8_t totalQueriesPerTimer = platParams->m_numFramesInFlight * 2; // x2 for start/end timestamps
		const GLsizei totalQuerySlots = totalQueriesPerTimer * re::GPUTimer::k_maxGPUTimersPerFrame;

		platParams->m_queryIDs.resize(totalQuerySlots, 0);

		glGenQueries(totalQuerySlots, platParams->m_queryIDs.data());

		for (size_t i = 0; i < platParams->m_queryIDs.size(); ++i)
		{
			// New query names are not associated with a query object until glBeginQuery
			glBeginQuery(GL_TIME_ELAPSED, platParams->m_queryIDs[i]);
			glEndQuery(GL_TIME_ELAPSED);

			SEAssert(glIsQuery(platParams->m_queryIDs[i]), "GPUTimer::Create failed to create OpenGL query object");

			glObjectLabel(GL_QUERY, platParams->m_queryIDs[i], -1, 
				std::format("GPUTimer{}:{}Query", i, i % 2 == 0 ? "Start" : "End").c_str());
		}

		platParams->m_invGPUFrequency = 1.f / 1000000.0; // OpenGL reports time in nanoseconds (ns)
	}


	void GPUTimer::Destroy(re::GPUTimer const& timer)
	{
		opengl::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<opengl::GPUTimer::PlatformParams*>();

		glDeleteQueries(util::CheckedCast<GLsizei>(platParams->m_queryIDs.size()), platParams->m_queryIDs.data());

		platParams->m_queryIDs.clear();
	}


	void GPUTimer::BeginFrame(re::GPUTimer const& timer)
	{
		// 
	}


	std::vector<uint64_t> GPUTimer::EndFrame(re::GPUTimer const& timer, void*)
	{
		opengl::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<opengl::GPUTimer::PlatformParams*>();

		const uint8_t frameIdx = (platParams->m_currentFrameNum % platParams->m_numFramesInFlight);

		// Readback our oldest queries:
		const uint32_t totalTimes = re::GPUTimer::k_maxGPUTimersPerFrame * 2;
		std::vector<uint64_t> gpuTimes(totalTimes, 0);

		const uint8_t oldestFrameIdx = (frameIdx + 1) % platParams->m_numFramesInFlight;
		const uint32_t queryStartOffset = oldestFrameIdx * re::GPUTimer::k_maxGPUTimersPerFrame * 2;

		for (size_t curQueryIdx = 0; curQueryIdx < totalTimes; curQueryIdx += 2) // Iterate over start/end pairs
		{
			const size_t curQueryStartIdx = queryStartOffset + curQueryIdx;
			const size_t curQueryEndIdx = curQueryStartIdx + 1;

			// Note: We don't check/wait for query results as they were issued in the previous frame

			// Get the query results:
			glGetQueryObjectui64v(platParams->m_queryIDs[curQueryStartIdx], GL_QUERY_RESULT, &gpuTimes[curQueryIdx]);
			glGetQueryObjectui64v(platParams->m_queryIDs[curQueryEndIdx], GL_QUERY_RESULT, &gpuTimes[curQueryIdx + 1]);
		}

		return gpuTimes;
	}


	void GPUTimer::StartTimer(re::GPUTimer const& timer, uint32_t startQueryIdx, void*)
	{
		opengl::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<opengl::GPUTimer::PlatformParams*>();

		glQueryCounter(platParams->m_queryIDs[startQueryIdx], GL_TIMESTAMP);
	}


	void GPUTimer::StopTimer(re::GPUTimer const& timer, uint32_t endQueryIdx, void*)
	{
		opengl::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<opengl::GPUTimer::PlatformParams*>();

		glQueryCounter(platParams->m_queryIDs[endQueryIdx], GL_TIMESTAMP);
	}
}