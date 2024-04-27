// © 2023 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Core\Util\CastUtils.h"
#include "SysInfo_OpenGL.h"


// Note: Most of these functions can/will likely be called from the main thread. Follow the pattern of caching the
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


	uint8_t SysInfo::GetMaxTextureBindPoints()
	{
		static uint8_t s_maxCombinedTexInputs = 0;
		if (s_maxCombinedTexInputs == 0)
		{
			GLint maxTexInputs = 0;
			glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTexInputs);
			s_maxCombinedTexInputs = util::CheckedCast<uint8_t>(maxTexInputs);
		}
		return s_maxCombinedTexInputs;
	}


	GLint SysInfo::GetMaxAnisotropy()
	{
		static GLint s_maxAnisotropy = 0;
		if (s_maxAnisotropy == 0)
		{
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &s_maxAnisotropy);
		}
		return s_maxAnisotropy;
	}
}