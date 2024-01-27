// © 2022 Adam Badke. All rights reserved.
#include "ParameterBlock_Platform.h"
#include "ParameterBlock.h"
#include "ParameterBlock_OpenGL.h"
#include "ParameterBlock_DX12.h"
#include "Assert.h"
#include "Config.h"

using en::Config;


namespace platform
{
	void platform::ParameterBlock::CreatePlatformParams(re::ParameterBlock& paramBlock)
	{
		SEAssert(paramBlock.GetPlatformParams() == nullptr,
			"Attempting to create platform params for a parameter block that already exists");

		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			paramBlock.SetPlatformParams(std::make_unique<opengl::ParameterBlock::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			paramBlock.SetPlatformParams(std::make_unique<dx12::ParameterBlock::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}

	// Function handles:
	void (*platform::ParameterBlock::Create)(re::ParameterBlock&) = nullptr;
	void (*platform::ParameterBlock::Update)(
		re::ParameterBlock const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes) = nullptr;
	void (*platform::ParameterBlock::Destroy)(re::ParameterBlock&) = nullptr;
}