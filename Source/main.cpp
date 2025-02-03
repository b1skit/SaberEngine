// © 2022 Adam Badke. All rights reserved.
#include "EngineApp.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Host/Window_Win32.h"


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	// Store the HINSTANCE for when we initialize our window
	win32::Window::s_platformState.m_hInstance = hInstance;

	core::Config::Get()->LoadConfigFile();
	core::Config::Get()->SetCommandLineArgs(__argc, __argv);

	LOG("\nWelcome to the Saber Engine!\n");

	app::EngineApp m_engineApp;

	m_engineApp.Startup();
	m_engineApp.Run();
	m_engineApp.Shutdown();

	return 0;
}
