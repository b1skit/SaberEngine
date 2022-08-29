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

	SaberEngine::CoreEngine coreEngine(argc, argv); // TODO: Implement config file (command line) path passing

	coreEngine.Startup();

	coreEngine.Run();

	coreEngine.Shutdown();

	LOG("\nGoodbye!\n");

	return 0;
}
