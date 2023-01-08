// © 2022 Adam Badke. All rights reserved.
#include "Window.h"
#include "Window_Platform.h"
#include "Window_Win32.h"
#include "DebugConfiguration.h"
#include "Config.h"


namespace platform
{
	using en::Config;


	void Window::PlatformParams::CreatePlatformParams(re::Window& window)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			window.m_platformParams = std::make_unique<win32::Window::PlatformParams>();
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


	bool (*platform::Window::Create)(re::Window& window, std::string const& title, uint32_t width, uint32_t height);
	void (*platform::Window::Destroy)(re::Window& window);
	void (*platform::Window::SetRelativeMouseMode)(re::Window const& window, bool enabled);
}