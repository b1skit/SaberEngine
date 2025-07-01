// © 2022 Adam Badke. All rights reserved.
#include "RenderManager_Platform.h"


namespace platform
{
	void (*RenderManager::Initialize)(re::RenderManager&) = nullptr;
	void (*RenderManager::Shutdown)(re::RenderManager&) = nullptr;
	void (*RenderManager::CreateAPIResources)(re::RenderManager&) = nullptr;
	void (*RenderManager::BeginFrame)(re::RenderManager&, uint64_t frameNum) = nullptr;
	void (*RenderManager::EndFrame)(re::RenderManager&) = nullptr;

	uint8_t(*RenderManager::GetNumFramesInFlight)() = nullptr;
}