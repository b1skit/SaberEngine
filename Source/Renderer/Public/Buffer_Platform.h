// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class Buffer;
}


namespace platform
{
	class Buffer
	{
	public:
		static void CreatePlatformObject(re::Buffer& buffer);


	public:
		static void (*Create)(re::Buffer&);
		static void (*Update)(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);

		static void const* (*MapCPUReadback)(re::Buffer const&, uint8_t frameLatency);
		static void (*UnmapCPUReadback)(re::Buffer const&);
	};
}