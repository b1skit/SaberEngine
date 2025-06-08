// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include <GL/glew.h>


namespace opengl
{
	class SysInfo
	{
	public:
		// Note: If you're adding a value here, make sure to call it in the opengl::Context::Create to cache it
		static uint8_t GetMaxRenderTargets();
		static uint8_t GetMaxTextureBindPoints();
		static uint8_t GetMaxVertexAttributes();
		static bool BindlessResourcesSupported();
		
		static uint32_t GetMaxUniformBufferBindings(re::Shader::ShaderType);
		static uint32_t GetMaxShaderStorageBlockBindings(re::Shader::ShaderType);

		// OpenGL-specific:
		static GLint GetUniformBufferOffsetAlignment();		
		static GLint GetShaderStorageBufferOffsetAlignment();
		static GLint GetMaxAnisotropy();
	};
}