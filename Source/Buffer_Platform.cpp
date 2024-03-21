// � 2022 Adam Badke. All rights reserved.
#include "Buffer_Platform.h"
#include "Buffer.h"
#include "Buffer_OpenGL.h"
#include "Buffer_DX12.h"
#include "Assert.h"
#include "Config.h"


namespace platform
{
	void platform::Buffer::CreatePlatformParams(re::Buffer& buffer)
	{
		SEAssert(buffer.GetPlatformParams() == nullptr,
			"Attempting to create platform params for a buffer that already exists");

		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			buffer.SetPlatformParams(std::make_unique<opengl::Buffer::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			buffer.SetPlatformParams(std::make_unique<dx12::Buffer::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}

	// Function handles:
	void (*platform::Buffer::Create)(re::Buffer&) = nullptr;
	void (*platform::Buffer::Update)(
		re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes) = nullptr;
	void (*platform::Buffer::Destroy)(re::Buffer&) = nullptr;
}