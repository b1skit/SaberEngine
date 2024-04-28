// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace definitions
{
	enum SEKeycode;
}
namespace en
{
	class InputManager;
}

namespace platform
{
	class InputManager
	{
	public:
		static void (*Startup)(en::InputManager&);
		static definitions::SEKeycode (*ConvertToSEKeycode)(uint32_t platKeycode);
	};
}