// © 2022 Adam Badke. All rights reserved.
#include "Core\Config.h"
#include "EngineApp.h"
#include "Core\Assert.h"
#include "Platform.h"
#include "Window_Win32.h"



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Store the HINSTANCE for when we initialize our window
	win32::Window::s_platformState.m_hInstance = hInstance;

	// Initialize Config from our pre-parsed argument vector
	int argc = __argc;
	char** argv = __argv;
	const bool gotCommandLineArgs = argc > 1;
	if (gotCommandLineArgs)
	{
		core::Config::Get()->ProcessCommandLineArgs(argc, argv);
	}	

	const bool showConsole = core::Config::Get()->KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg);
	if (showConsole)
	{
		AllocConsole();
		freopen("CONOUT$", "wb", stdout);
	}

	if (gotCommandLineArgs)
	{
		const int numTokens = argc - 1; // -1, as 1st arg is program name
		LOG("Received %d command line tokens: %s",
			numTokens, 
			core::Config::Get()->GetValueAsString(core::configkeys::k_commandLineArgsValueKey).c_str());
	}
	else
	{
		LOG("No command line arguments received");
	}

	core::Config::Get()->LoadConfigFile();

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

	LOG("\nGoodbye!\n");

	if (showConsole)
	{
		FreeConsole();
		fclose(stdout);
	}

	return 0;
}
