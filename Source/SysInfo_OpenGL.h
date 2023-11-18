// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace opengl
{
	class SysInfo
	{
	public:
		// Note: If you're adding a value here, make sure to call it in the opengl::Context::Create to cache it
		static uint8_t GetMaxRenderTargets();
		static uint8_t GetMaxVertexAttributes();
		static GLint GetUniformBufferOffsetAlignment();
		static GLint GetShaderStorageBufferOffsetAlignment();
	};
}