// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager_DX12.h"
#include "BindlessResourceManager_Platform.h"

#include "Core/Config.h"


namespace platform
{
	std::unique_ptr<re::BindlessResourceManager::PlatObj>
		BindlessResourceManager::CreatePlatformObject()
	{
		const platform::RenderingAPI api =
			core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			SEAssertF("Invalid rendering API: OpenGL does not (currently) support bindless resources in any form");
		}
		break;
		case RenderingAPI::DX12:
		{
			return std::make_unique<dx12::BindlessResourceManager::PlatObj>();
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}
		return nullptr; // This should never happen
	}

	void (*IBindlessResource::GetResourceUseState)(void* dest, size_t destByteSize);


	void (*BindlessResourceManager::Initialize)(re::BindlessResourceManager&, uint8_t numFramesInFlight, uint64_t frameNum) = nullptr;
	void (*BindlessResourceManager::SetResource)(re::BindlessResourceManager&, re::IBindlessResource*, ResourceHandle) = nullptr;
}