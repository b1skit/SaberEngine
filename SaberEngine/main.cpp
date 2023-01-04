// © 2022 Adam Badke. All rights reserved.
#include "Platform.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"

#include <SDL.h> // Need to include this here so SDL can find our main function


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Display a console in debug mode
#if defined(_DEBUG)
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
	en::CoreEngine m_coreEngine(argc, argv); // TODO: Implement config file (command line) path passing

	m_coreEngine.Startup();

	m_coreEngine.Run();

	m_coreEngine.Shutdown();

	LOG("\nGoodbye!\n");

	return 0;
}
