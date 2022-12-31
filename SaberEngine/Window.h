#pragma once

#include "Window_Platform.h"


namespace re
{
	class Window
	{
	public:
		Window();
		
		~Window() { Destroy(); };

		bool Create(std::string const& title, uint32_t width, uint32_t height);
		void Destroy();

		void Present() const;
		bool HasFocus() const;

		platform::Window::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }


	private:
		std::unique_ptr<platform::Window::PlatformParams> m_platformParams;


	private:
		// Copying not allowed
		Window(Window const&) = delete;
		Window(Window&&) = delete;
		Window& operator=(Window const&) = delete;


	private:
		friend void platform::Window::PlatformParams::CreatePlatformParams(re::Window&);
	};
}
