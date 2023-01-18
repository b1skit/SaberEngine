// © 2022 Adam Badke. All rights reserved.
#include "Window_Platform.h"
#include "Window_Win32.h"
#include "DebugConfiguration.h"
#include "Config.h"


namespace platform
{
	using en::Config;


	void Window::CreatePlatformParams(re::Window& window)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			window.SetPlatformParams(std::make_unique<win32::Window::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			SEAssertF("DX12 is not yet supported");
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	bool (*platform::Window::Create)(re::Window& window, std::string const& title, uint32_t width, uint32_t height) = nullptr;
	void (*platform::Window::Destroy)(re::Window& window) = nullptr;
	void (*platform::Window::SetRelativeMouseMode)(re::Window const& window, bool enabled) = nullptr;
}