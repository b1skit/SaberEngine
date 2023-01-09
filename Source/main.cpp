// © 2022 Adam Badke. All rights reserved.
#include "Platform.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Window_Win32.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Store the HINSTANCE for when we initialize our window
	win32::Window::PlatformState.m_hInstance = hInstance;

#if defined(_DEBUG)
	// Display a Win32 console in debug mode
	AllocConsole();
	freopen("CONOUT$", "wb", stdout);
#endif

	// Register our API-specific bindings before anything attempts to call them:
	if (!platform::RegisterPlatformFunctions())
	{
		LOG_ERROR("Failed to configure API-specific platform bindings!\n");
		exit(-1);
	}

	LOG("\nWelcome to the Saber Engine!\n");

	// Get our pre-parsed argument vector
	int argc = __argc;
	char** argv = __argv;	
	en::CoreEngine m_coreEngine(argc, argv); // TODO: Implement command line config file path passing

	m_coreEngine.Startup();
	m_coreEngine.Run();
	m_coreEngine.Shutdown();

	LOG("\nGoodbye!\n");

#if defined(_DEBUG)
	FreeConsole();
	fclose(stdout);
#endif

	return 0;
}
