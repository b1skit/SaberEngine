// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	class EventManager;
}

namespace platform
{
	class EventManager
	{
	public:
		static void (*ProcessMessages)(en::EventManager& eventManager);
	};
}