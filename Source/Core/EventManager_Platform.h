// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	class EventManager;
}

namespace platform
{
	class EventManager
	{
	public:
		static void (*ProcessMessages)(core::EventManager& eventManager);
	};
}