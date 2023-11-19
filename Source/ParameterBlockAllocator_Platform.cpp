// © 2023 Adam Badke. All rights reserved.
#include "Config.h"
#include "Assert.h"
#include "ParameterBlockAllocator.h"
#include "ParameterBlockAllocator_DX12.h"
#include "ParameterBlockAllocator_OpenGL.h"
#include "ParameterBlockAllocator_Platform.h"


namespace platform
{
	void ParameterBlockAllocator::CreatePlatformParams(re::ParameterBlockAllocator& pba)
	{
		SEAssert("Platform params already exists", pba.GetPlatformParams() == nullptr);

		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			pba.SetPlatformParams(std::make_unique<opengl::ParameterBlockAllocator::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			pba.SetPlatformParams(std::make_unique<dx12::ParameterBlockAllocator::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void (*platform::ParameterBlockAllocator::Create)(re::ParameterBlockAllocator&) = nullptr;
	void (*platform::ParameterBlockAllocator::Destroy)(re::ParameterBlockAllocator&) = nullptr;
}