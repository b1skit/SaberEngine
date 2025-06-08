// © 2022 Adam Badke. All rights reserved.
#include "Private/EventManager_Platform.h"


namespace platform
{
	void (*platform::EventManager::ProcessMessages)(core::EventManager& eventManager) = nullptr;
}