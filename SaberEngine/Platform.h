#pragma once

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

namespace platform
{
	enum RenderingAPI
	{
		OpenGL,
		DX12,
		RenderingAPI_Count
	};


	// Configure API-specific bindings:
	extern bool RegisterPlatformFunctions();
}
