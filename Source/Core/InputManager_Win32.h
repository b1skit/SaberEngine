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

namespace win32
{
	class InputManager
	{
	public:
		static void Startup(en::InputManager& inputManager);
		static definitions::SEKeycode ConvertToSEKeycode(uint32_t platKeycode);
	};
}