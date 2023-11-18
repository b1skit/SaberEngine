// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace platform
{
	class ParameterBlockAllocator
	{
	public:
		static void CreatePlatformParams(re::ParameterBlockAllocator&);


	public:
		static void (*Create)(re::ParameterBlockAllocator&);
		static void (*Destroy)(re::ParameterBlockAllocator&);
	};
}