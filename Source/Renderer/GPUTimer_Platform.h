// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "GPUTimer.h"


namespace platform
{
	class GPUTimer
	{
	public:
		static std::unique_ptr<re::GPUTimer::PlatformParams> CreatePlatformParams();

	public:
		static void (*Create)(re::GPUTimer const&);
		static void (*Destroy)(re::GPUTimer const&);

		static void (*BeginFrame)(re::GPUTimer const&);
		static std::vector<uint64_t>(*EndFrame)(re::GPUTimer const&, void*);

		static void (*StartTimer)(re::GPUTimer const&, uint32_t startQueryIdx, void*);
		static void (*StopTimer)(re::GPUTimer const&, uint32_t endQueryIdx, void*);
	};
}