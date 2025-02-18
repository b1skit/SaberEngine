// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"


namespace platform
{
	class AccelerationStructure
	{
	public:
		static std::unique_ptr<re::AccelerationStructure::PlatformParams> CreatePlatformParams();


	public:
		static void (*Create)(re::AccelerationStructure&);
		static void (*Destroy)(re::AccelerationStructure&);
	};
}