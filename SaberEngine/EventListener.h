#pragma once

#include <memory>

#include "EventManager.h"


namespace en
{
	class EventListener
	{
	public:
		virtual void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) = 0;
	};

}