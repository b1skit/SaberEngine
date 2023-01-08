// © 2022 Adam Badke. All rights reserved.

namespace en
{
	class EventManager;
}

namespace win32
{
	class EventManager
	{
	public:
		static void ProcessMessages(en::EventManager& eventManager);
	};
}