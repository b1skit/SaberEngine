#include <iostream>
#include "CoreEngine.h"


int main(int argc, char** argv)
{
	std::cout << "Welcome to the Saber Engine!\n\n";

	SaberEngine::CoreEngine coreEngine(argc, argv); // TODO: Implement config file (command line) path passing

	coreEngine.Startup();

	coreEngine.Run();

	coreEngine.Shutdown();

	std::cout << "\nGoodbye!\n";

	return 0;
}
