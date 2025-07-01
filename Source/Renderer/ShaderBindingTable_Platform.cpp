// © 2025 Adam Badke. All rights reserved.
#include "ShaderBindingTable_DX12.h"
#include "ShaderBindingTable_Platform.h"

#include "Core/Config.h"


namespace platform
{
	std::unique_ptr<re::ShaderBindingTable::PlatObj> platform::ShaderBindingTable::CreatePlatformObject()
	{
		const platform::RenderingAPI api =
			core::Config::Get()->GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			SEAssertF("OpenGL does not support ray tracing. Creating a ShaderBindingTable is unexpected");
		}
		break;
		case RenderingAPI::DX12:
		{
			return std::make_unique<dx12::ShaderBindingTable::PlatObj>();
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}
		return nullptr; // This should never happen
	}


	void (*ShaderBindingTable::Create)(re::ShaderBindingTable&) = nullptr;
}