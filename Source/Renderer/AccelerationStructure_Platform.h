// � 2025 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"


namespace platform
{
	class AccelerationStructure
	{
	public:
		static std::unique_ptr<gr::AccelerationStructure::PlatObj> CreatePlatformObject();


	public:
		static void (*Create)(gr::AccelerationStructure&);
		static void (*Destroy)(gr::AccelerationStructure&);
	};
}