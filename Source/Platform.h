// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace platform
{
	enum RenderingAPI : uint8_t
	{
		DX12,
		OpenGL,
		RenderingAPI_Count
	};
	extern constexpr char const* RenderingAPIToCStr(platform::RenderingAPI);

	bool RegisterPlatformFunctions(); // Configure API-specific bindings
}
