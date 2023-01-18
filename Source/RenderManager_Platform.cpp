// © 2022 Adam Badke. All rights reserved.
#include "RenderManager_Platform.h"


namespace platform
{
	void (*RenderManager::Initialize)(re::RenderManager&) = nullptr;
	void (*RenderManager::Render)(re::RenderManager&) = nullptr;
	void (*RenderManager::RenderImGui)(re::RenderManager&) = nullptr;
	void (*RenderManager::Shutdown)(re::RenderManager&) = nullptr;
}