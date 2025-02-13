// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class AccelerationStructureManager;
}

namespace platform
{
	class AccelerationStructureManager
	{
	public:
		static void CreatePlatformParams(re::AccelerationStructureManager&);

	public:
		static void(*Create)(re::AccelerationStructureManager&);
		static void(*Update)(re::AccelerationStructureManager&);
		static void(*Destroy)(re::AccelerationStructureManager&);
	};
}