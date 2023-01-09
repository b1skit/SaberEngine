// © 2022 Adam Badke. All rights reserved.
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
