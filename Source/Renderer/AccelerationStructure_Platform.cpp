// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure_DX12.h"
#include "AccelerationStructure_Platform.h"
#include "RenderManager.h"


namespace platform
{
	std::unique_ptr<re::AccelerationStructure::PlatObj> platform::AccelerationStructure::CreatePlatformObject()
	{
		const platform::RenderingAPI api = gr::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			SEAssertF("OpenGL does not support ray tracing. Creating an AccelerationStructure is unexpected");
		}
		break;
		case RenderingAPI::DX12:
		{
			return std::make_unique<dx12::AccelerationStructure::PlatObj>();
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}

		return nullptr; // This should never happen
	}


	void (*AccelerationStructure::Create)(re::AccelerationStructure&) = nullptr;
	void (*AccelerationStructure::Destroy)(re::AccelerationStructure&) = nullptr;
}