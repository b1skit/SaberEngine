// © 2023 Adam Badke. All rights reserved.
#include "Config.h"
#include "Assert.h"
#include "BufferAllocator.h"
#include "BufferAllocator_DX12.h"
#include "BufferAllocator_OpenGL.h"
#include "BufferAllocator_Platform.h"


namespace platform
{
	void BufferAllocator::CreatePlatformParams(re::BufferAllocator& ba)
	{
		SEAssert(ba.GetPlatformParams() == nullptr, "Platform params already exists");

		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			ba.SetPlatformParams(std::make_unique<opengl::BufferAllocator::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			ba.SetPlatformParams(std::make_unique<dx12::BufferAllocator::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void (*platform::BufferAllocator::Create)(re::BufferAllocator&) = nullptr;
	void (*platform::BufferAllocator::Destroy)(re::BufferAllocator&) = nullptr;
}