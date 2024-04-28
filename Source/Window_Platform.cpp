// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "RenderManager.h"
#include "Window_Platform.h"
#include "Window_Win32.h"


namespace platform
{
	void Window::CreatePlatformParams(en::Window& window)
	{
		// TODO: We only support windows for now, but eventually the Window interface should be decided by the
		// OS/platform, not the rendering API.
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			window.SetPlatformParams(std::make_unique<win32::Window::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			window.SetPlatformParams(std::make_unique<win32::Window::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	bool (*platform::Window::Create)(en::Window& window, std::string const& title, uint32_t width, uint32_t height) = nullptr;
	void (*platform::Window::Destroy)(en::Window& window) = nullptr;
	void (*platform::Window::SetRelativeMouseMode)(en::Window const& window, bool enabled) = nullptr;
}