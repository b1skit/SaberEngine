// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class Buffer;
	class IBufferAllocatorAccess;
}

namespace platform
{
	class Buffer
	{
	public:
		static void CreatePlatformObject(re::Buffer& buffer);


	public:
		static void (*Create)(re::Buffer&, re::IBufferAllocatorAccess*, uint8_t numFramesInFlight);
		static void (*Update)(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);

		static void const* (*MapCPUReadback)(re::Buffer const&, re::IBufferAllocatorAccess const*, uint8_t frameLatency);
		static void (*UnmapCPUReadback)(re::Buffer const&, re::IBufferAllocatorAccess const*);
	};
}