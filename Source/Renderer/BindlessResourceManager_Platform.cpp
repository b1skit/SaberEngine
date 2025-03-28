// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager_DX12.h"
#include "BindlessResourceManager_Platform.h"
#include "RenderManager.h"


namespace platform
{
	std::unique_ptr<re::IBindlessResourceSet::PlatformParams>
		IBindlessResourceSet::CreatePlatformParams()
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			SEAssertF("Invalid rendering API: OpenGL does not (currently) support bindless resources in any form");
		}
		break;
		case RenderingAPI::DX12:
		{
			return std::make_unique<dx12::IBindlessResourceSet::PlatformParams>();
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}
		return nullptr; // This should never happen
	}


	void (*IBindlessResourceSet::Initialize)(re::IBindlessResourceSet&) = nullptr;
	void (*IBindlessResourceSet::SetResource)(re::IBindlessResourceSet&, re::IBindlessResource*, ResourceHandle) = nullptr;
}