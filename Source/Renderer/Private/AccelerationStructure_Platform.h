// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Private/AccelerationStructure.h"


namespace platform
{
	class AccelerationStructure
	{
	public:
		static std::unique_ptr<re::AccelerationStructure::PlatObj> CreatePlatformObject();


	public:
		static void (*Create)(re::AccelerationStructure&);
		static void (*Destroy)(re::AccelerationStructure&);
	};
}