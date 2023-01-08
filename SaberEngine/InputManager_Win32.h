// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	enum SEKeycode;
	class InputManager;
}

namespace win32
{
	class InputManager
	{
	public:
		static void Startup(en::InputManager& inputManager);
		static en::SEKeycode ConvertToSEKeycode(uint32_t platKeycode);
	};
}