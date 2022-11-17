#include "RenderManager_Platform.h"


namespace platform
{
	void (*RenderManager::Initialize)(re::RenderManager&);
	void (*RenderManager::StartOfFrame)();
	void (*RenderManager::Render)(re::RenderManager&);	
}