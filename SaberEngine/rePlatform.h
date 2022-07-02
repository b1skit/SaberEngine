#pragma once


namespace re::platform
{
	enum RenderingAPI
	{
		OpenGL,
		DX12,
		RenderingAPI_Count
	};

	// Configure Graphics API-specific bindings
	bool RegisterPlatformFunctions();
}
