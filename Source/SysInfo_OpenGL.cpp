// © 2023 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/wglew.h> // Windows-specific GL functions and macros
#include <GL/GL.h> // Must follow glew.h

#include "SysInfo_OpenGL.h"


namespace opengl
{
	uint8_t SysInfo::GetMaxRenderTargets()
	{
		GLint maxColorAttachments = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
		return maxColorAttachments;
	}
}