// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	enum SEKeycode;
	class InputManager;
}

namespace platform
{
	class InputManager
	{
	public:
		static void (*Startup)(en::InputManager&);
		static en::SEKeycode (*ConvertToSEKeycode)(uint32_t platKeycode);
	};
}