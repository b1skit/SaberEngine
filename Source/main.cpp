// © 2022 Adam Badke. All rights reserved.
#include "EngineApp.h"
#include "Platform.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Host/Window_Win32.h"


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	// Store the HINSTANCE for when we initialize our window
	win32::Window::s_platformState.m_hInstance = hInstance;

	core::Config::Get()->LoadConfigFile();
	core::Config::Get()->SetCommandLineArgs(__argc, __argv);

	// Register our API-specific bindings before anything attempts to call them:
	if (!platform::RegisterPlatformFunctions())
	{
		LOG_ERROR("Failed to configure API-specific platform bindings!\n");
		exit(-1);
	}

	LOG("\nWelcome to the Saber Engine!\n");

	app::EngineApp m_engineApp;

	m_engineApp.Startup();
	m_engineApp.Run();
	m_engineApp.Shutdown();

	return 0;
}
