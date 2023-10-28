// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace opengl
{
	class SysInfo
	{
	public:
		static uint8_t GetMaxRenderTargets();
		static uint8_t GetMaxVertexAttributes();
	};
}