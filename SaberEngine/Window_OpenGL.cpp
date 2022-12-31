#include <SDL.h>

#include "Window.h"
#include "Window_OpenGL.h"
#include "DebugConfiguration.h"


namespace opengl
{
	bool Window::Create(re::Window& window, std::string const& title, uint32_t width, uint32_t height)
	{
		opengl::Window::PlatformParams* const platformParams =
			dynamic_cast<opengl::Window::PlatformParams*>(window.GetPlatformParams());

		platformParams->m_glWindow = SDL_CreateWindow(
			title.c_str(),
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width,
			height,
			SDL_WINDOW_OPENGL);

		SEAssert("Could not create window", platformParams->m_glWindow != NULL);
		
		return platformParams->m_glWindow != NULL;
	}


	void Window::Destroy(re::Window& window)
	{
		opengl::Window::PlatformParams* const platformParams =
			dynamic_cast<opengl::Window::PlatformParams*>(window.GetPlatformParams());

		SDL_DestroyWindow(platformParams->m_glWindow);
	}


	void Window::Present(re::Window const& window)
	{
		opengl::Window::PlatformParams const* const platformParams =
			dynamic_cast<opengl::Window::PlatformParams const*>(window.GetPlatformParams());

		SDL_GL_SwapWindow(platformParams->m_glWindow);
	}


	bool Window::HasFocus(re::Window const& window)
	{
		opengl::Window::PlatformParams const* contextPlatformParams =
			dynamic_cast<opengl::Window::PlatformParams const*>(window.GetPlatformParams());

		const uint32_t windowFlags = SDL_GetWindowFlags(contextPlatformParams->m_glWindow);

		return (windowFlags & (SDL_WINDOW_INPUT_FOCUS));
	}
}