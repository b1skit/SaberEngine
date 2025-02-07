// © 2025 Adam Badke. All rights reserved.
#include "GPUTimer_DX12.h"
#include "GPUTimer_OpenGL.h"
#include "GPUTimer_Platform.h"
#include "RenderManager.h"


namespace platform
{
	std::unique_ptr<re::GPUTimer::PlatformParams> platform::GPUTimer::CreatePlatformParams()
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			return std::make_unique<opengl::GPUTimer::PlatformParams>();
		}
		break;
		case RenderingAPI::DX12:
		{
			return std::make_unique<dx12::GPUTimer::PlatformParams>();
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}

		return nullptr; // This should never happen
	}


	void (*GPUTimer::Create)(re::GPUTimer const&) = nullptr;
	void (*GPUTimer::Destroy)(re::GPUTimer const&) = nullptr;

	void (*GPUTimer::BeginFrame)(re::GPUTimer const&) = nullptr;
	std::vector<uint64_t> (*GPUTimer::EndFrame)(re::GPUTimer const&, void*) = nullptr;

	void (*GPUTimer::StartTimer)(re::GPUTimer const&, uint32_t queryIdx, void*) = nullptr;
	void (*GPUTimer::StopTimer)(re::GPUTimer const&, uint32_t queryIdx, void*) = nullptr;
}