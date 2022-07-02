#include <SDL.h>

#include "BuildConfiguration.h"
#include "enPlatform.h"


namespace en::platform
{
	bool ConfigureEnginePlatform()
	{
		// Initialize SDL:
		if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
		{
			SaberEngine::LOG_ERROR(SDL_GetError());
			return false;
		}
		return true;
	}
}