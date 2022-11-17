#pragma once

#include <memory>

#include "EventManager.h"


namespace en
{
	class EventListener
	{
	public:
		virtual void HandleEvent(en::EventManager::EventInfo const& eventInfo) = 0;

	private:
		// TODO: Maintain a queue of (thread-safe written) incoming events (by value)
	};

}