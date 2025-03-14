// © 2025 Adam Badke. All rights reserved.
#include "RenderManager.h"
#include "ShaderBindingTable_DX12.h"
#include "ShaderBindingTable_Platform.h"


namespace platform
{
	std::unique_ptr<re::ShaderBindingTable::PlatformParams> platform::ShaderBindingTable::CreatePlatformParams()
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			SEAssertF("OpenGL does not support ray tracing. Creating a ShaderBindingTable is unexpected");
		}
		break;
		case RenderingAPI::DX12:
		{
			return std::make_unique<dx12::ShaderBindingTable::PlatformParams>();
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}
		return nullptr; // This should never happen
	}


	void (*ShaderBindingTable::Create)(re::ShaderBindingTable&) = nullptr;
}