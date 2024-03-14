// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace platform
{
	class BufferAllocator
	{
	public:
		static void CreatePlatformParams(re::BufferAllocator&);


	public:
		static void (*Create)(re::BufferAllocator&);
		static void (*Destroy)(re::BufferAllocator&);
	};
}