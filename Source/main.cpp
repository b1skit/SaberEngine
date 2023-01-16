// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Platform.h"
#include "Window_Win32.h"



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Store the HINSTANCE for when we initialize our window
	win32::Window::PlatformState.m_hInstance = hInstance;

	// Initialize Config from our pre-parsed argument vector
	int argc = __argc;
	char** argv = __argv;
	const bool gotCommandLineArgs = en::Config::Get()->ProcessCommandLineArgs(argc, argv);

	const bool showConsole = 
		en::Config::Get()->ValueExists(en::Config::k_showSystemConsoleWindowCommand) || !gotCommandLineArgs;

	if (showConsole)
	{
		AllocConsole();
		freopen("CONOUT$", "wb", stdout);

		// TODO: If no command line args are received, load into an empty scene
		if (!gotCommandLineArgs)
		{
			LOG_ERROR("No command line arguments received");
			system("pause");
			
			FreeConsole();
			fclose(stdout);
			exit(-1);
		}
	}

	// Register our API-specific bindings before anything attempts to call them:
	if (!platform::RegisterPlatformFunctions())
	{
		LOG_ERROR("Failed to configure API-specific platform bindings!\n");
		exit(-1);
	}

	LOG("\nWelcome to the Saber Engine!\n");

	en::CoreEngine m_coreEngine; // TODO: Implement command line config file path passing

	m_coreEngine.Startup();
	m_coreEngine.Run();
	m_coreEngine.Shutdown();

	LOG("\nGoodbye!\n");

	if (showConsole)
	{
		FreeConsole();
		fclose(stdout);
	}

	return 0;
}
