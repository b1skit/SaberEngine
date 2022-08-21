#pragma once

#include <memory>

namespace SaberEngine
{
	// Predeclaration:
	struct EventInfo;

	class EventListener
	{
	public:
		virtual void HandleEvent(std::shared_ptr<EventInfo const> eventInfo) = 0;

	private:

	};

}