// © 2022 Adam Badke. All rights reserved.
#include "InputManager_Platform.h"


namespace platform
{
	void (*platform::InputManager::Startup)(en::InputManager&) = nullptr;
	definitions::SEKeycode(*platform::InputManager::ConvertToSEKeycode)(uint32_t platKeycode) = nullptr;
}