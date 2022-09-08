#include <iostream>

#include "Platform.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"


int main(int argc, char** argv)
{
	// Register our API-specific bindings before anything attempts to call them:
	if (!platform::RegisterPlatformFunctions())
	{
		LOG_ERROR("Failed to configure API-specific platform bindings!\n");
		exit(-1);
	}

	LOG("\nWelcome to the Saber Engine!\n");

	en::CoreEngine m_coreEngine(argc, argv); // TODO: Implement config file (command line) path passing

	m_coreEngine.Startup();

	m_coreEngine.Run();

	m_coreEngine.Shutdown();

	LOG("\nGoodbye!\n");

	return 0;
}
