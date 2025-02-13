// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructureManager.h"
#include "AccelerationStructureManager_DX12.h"
#include "AccelerationStructureManager_Platform.h"
#include "RenderManager.h"


namespace platform
{
	void AccelerationStructureManager::CreatePlatformParams(re::AccelerationStructureManager& asMgr)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			SEAssertF("Trying to create AccelerationStructureManager platform params when the rendering API is OpenGL. "
				"This is unexpected ");
		}
		break;
		case RenderingAPI::DX12:
		{
			asMgr.SetPlatformParams(std::make_unique<dx12::AccelerationStructureManager::PlatformParams>());
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}
	}


	void(*AccelerationStructureManager::Create)(re::AccelerationStructureManager&) = nullptr;
	void(*AccelerationStructureManager::Update)(re::AccelerationStructureManager&) = nullptr;
	void(*AccelerationStructureManager::Destroy)(re::AccelerationStructureManager&) = nullptr;
}