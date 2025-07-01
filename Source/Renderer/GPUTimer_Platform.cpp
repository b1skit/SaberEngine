// © 2025 Adam Badke. All rights reserved.
#include "EnumTypes.h"
#include "GPUTimer_DX12.h"
#include "GPUTimer_OpenGL.h"
#include "GPUTimer_Platform.h"

#include "Core/Config.h"


namespace platform
{
	std::unique_ptr<re::GPUTimer::PlatObj> platform::GPUTimer::CreatePlatformObject()
	{
		const platform::RenderingAPI api =
			core::Config::Get()->GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			return std::make_unique<opengl::GPUTimer::PlatObj>();
		}
		break;
		case RenderingAPI::DX12:
		{
			return std::make_unique<dx12::GPUTimer::PlatObj>();
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}

		return nullptr; // This should never happen
	}


	void (*GPUTimer::Create)(re::GPUTimer const&) = nullptr;

	void (*GPUTimer::BeginFrame)(re::GPUTimer const&) = nullptr;
	std::vector<uint64_t> (*GPUTimer::EndFrame)(re::GPUTimer const&, re::GPUTimer::TimerType) = nullptr;

	void (*GPUTimer::StartTimer)(re::GPUTimer const&, re::GPUTimer::TimerType, uint32_t queryIdx, void*) = nullptr;
	void (*GPUTimer::StopTimer)(re::GPUTimer const&, re::GPUTimer::TimerType, uint32_t queryIdx, void*) = nullptr;
}