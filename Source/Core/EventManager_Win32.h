// © 2022 Adam Badke. All rights reserved.

namespace core
{
	class EventManager;
}

namespace win32
{
	class EventManager
	{
	public:
		static void ProcessMessages(core::EventManager& eventManager);
	};
}