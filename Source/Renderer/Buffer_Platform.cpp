// © 2022 Adam Badke. All rights reserved.
#include "Buffer_Platform.h"
#include "Buffer.h"
#include "Buffer_OpenGL.h"
#include "Buffer_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"


namespace platform
{
	void platform::Buffer::CreatePlatformObject(re::Buffer& buffer)
	{
		SEAssert(buffer.GetPlatformObject() == nullptr,
			"Attempting to create platform object for a buffer that already exists");

		const platform::RenderingAPI api =
			core::Config::Get()->GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			buffer.SetPlatformObject(std::make_unique<opengl::Buffer::PlatObj>());
		}
		break;
		case RenderingAPI::DX12:
		{
			buffer.SetPlatformObject(std::make_unique<dx12::Buffer::PlatObj>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}

	// Function handles:
	void (*platform::Buffer::Create)(re::Buffer&, re::IBufferAllocatorAccess*, uint8_t numFramesInFlight) = nullptr;
	void (*platform::Buffer::Update)(
		re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes) = nullptr;

	void const* (*platform::Buffer::MapCPUReadback)(re::Buffer const&, re::IBufferAllocatorAccess const*, uint8_t frameLatency) = nullptr;
	void (*platform::Buffer::UnmapCPUReadback)(re::Buffer const&, re::IBufferAllocatorAccess const*) = nullptr;
}