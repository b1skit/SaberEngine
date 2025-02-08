// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "GPUTimer.h"

#include <GL/glew.h>


namespace platform
{
	class GPUTimer;
}

namespace opengl
{
	class GPUTimer
	{
	public:
		struct PlatformParams final : public re::GPUTimer::PlatformParams
		{
			void Destroy() override;

			// OpenGL handles the difference in direct/compute and copy queues for us; we maintain seperate queries to
			// simplify the platform API
			std::vector<GLuint> m_directComputeQueryIDs;
			std::vector<GLuint> m_copyQueryIDs;
		};


	public:
		static void Create(re::GPUTimer const&);
		// Destroy is handled via GPUTimer::PlatformParams

		static void BeginFrame(re::GPUTimer const&);
		static std::vector<uint64_t> EndFrame(re::GPUTimer const&, re::GPUTimer::TimerType);

		static void StartTimer(re::GPUTimer const&, re::GPUTimer::TimerType, uint32_t startQueryIdx, void*);
		static void StopTimer(re::GPUTimer const&, re::GPUTimer::TimerType, uint32_t endQueryIdx, void*);
	};
}