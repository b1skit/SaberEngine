#include "RenderManager_Platform.h"


namespace platform
{
	void (*RenderManager::Initialize)(re::RenderManager&);
	void (*RenderManager::Render)(re::RenderManager&);
	void (*RenderManager::RenderImGui)(re::RenderManager&);
}