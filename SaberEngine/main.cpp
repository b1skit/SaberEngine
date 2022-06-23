#include <iostream>
#include "CoreEngine.h"

//using SaberEngine::CoreEngine;
using std::cout;


int main(int argc, char **argv)
{
	cout << "Welcome to the Saber Engine!\n\n";

	SaberEngine::CoreEngine coreEngine(argc, argv); // TODO: Implement config file (command line) path passing

	coreEngine.Startup();

	coreEngine.Run();

	coreEngine.Shutdown();

	cout << "\nGoodbye!\n";

	return 0;
}