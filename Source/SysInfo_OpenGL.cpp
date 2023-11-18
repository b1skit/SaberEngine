// © 2023 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/wglew.h> // Windows-specific GL functions and macros
#include <GL/GL.h> // Must follow glew.h

#include "DebugConfiguration.h"
#include "SysInfo_OpenGL.h"

// Note: Most of these functions can likely only be called from the main thread. Follow the pattern of caching the
// result in a static variable and priming it from the main thread during startup by calling from the opengl::Context
namespace opengl
{
	uint8_t SysInfo::GetMaxRenderTargets()
	{
		// NOTE: This can only be called from the main thread, so we cache the result in a static variable and call this
		// during opengl::Context initialization
		static GLint s_maxColorAttachments = 0;
		if (s_maxColorAttachments == 0)
		{
			glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &s_maxColorAttachments);
		}
		return s_maxColorAttachments;
	}


	uint8_t SysInfo::GetMaxVertexAttributes()
	{
		static GLint s_maxVertexAttributes = 0;
		if (s_maxVertexAttributes == 0)
		{
			glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &s_maxVertexAttributes);
		}
		return s_maxVertexAttributes;
	}


	GLint SysInfo::GetUniformBufferOffsetAlignment()
	{
		static GLint s_uniformBufferOffsetAlignment = 0;
		if (s_uniformBufferOffsetAlignment == 0)
		{
			glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s_uniformBufferOffsetAlignment);
		}
		return s_uniformBufferOffsetAlignment;
	}


	GLint SysInfo::GetShaderStorageBufferOffsetAlignment()
	{
		static GLint s_shaderStorageBufferOffsetAlignment = 0;
		if (s_shaderStorageBufferOffsetAlignment == 0)
		{
			glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &s_shaderStorageBufferOffsetAlignment);
		}
		return s_shaderStorageBufferOffsetAlignment;
	}
}