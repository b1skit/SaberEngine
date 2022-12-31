#pragma once

#include <SDL.h>

#include "Window_Platform.h"


namespace opengl
{
	class Window
	{
	public:
		struct PlatformParams final : public virtual platform::Window::PlatformParams
		{
			PlatformParams() = default;
			~PlatformParams() override = default;

			SDL_Window* m_glWindow = 0;
		};


	public:
		static bool Create(re::Window& window, std::string const& title, uint32_t width, uint32_t height);
		static void Destroy(re::Window& window);
		static void Present(re::Window const& window);
		static bool HasFocus(re::Window const& window);
	};
}