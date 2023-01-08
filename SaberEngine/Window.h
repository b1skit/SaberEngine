// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Window_Platform.h"


namespace re
{
	class Window
	{
	public:
		Window();
		
		~Window() { Destroy(); };

		void SetFocusState(bool hasFocus); // To be called by event handlers only
		bool GetFocusState() const;

		platform::Window::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }

		// Platform wrappers:
		bool Create(std::string const& title, uint32_t width, uint32_t height);
		void Destroy();
		void SetRelativeMouseMode(bool enabled) const; // Hides cursor and wraps movements around boundaries


	private:
		std::unique_ptr<platform::Window::PlatformParams> m_platformParams;
		bool m_hasFocus;

	private:
		// Copying not allowed
		Window(Window const&) = delete;
		Window(Window&&) = delete;
		Window& operator=(Window const&) = delete;


	private:
		friend void platform::Window::PlatformParams::CreatePlatformParams(re::Window&);
	};
}
