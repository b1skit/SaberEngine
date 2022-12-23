#pragma once


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
