// © 2022 Adam Badke. All rights reserved.
#include "EventManager_Win32.h"


namespace win32
{
	void EventManager::ProcessMessages(core::EventManager& eventManager)
	{
		MSG msg;
		while (::PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) // Peek (vs Get) doesn't wait for a message if none exists
		{
			// Re-broadcast the message to our win32::Window::WindowEventCallback handler
			TranslateMessage(&msg); // Translates virtual-key messages into character messages
			DispatchMessageA(&msg);
		}
	}
}