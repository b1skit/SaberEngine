// © 2022 Adam Badke. All rights reserved.
#include "EventManager_Platform.h"


namespace platform
{
	void (*platform::EventManager::ProcessMessages)(en::EventManager& eventManager);
}