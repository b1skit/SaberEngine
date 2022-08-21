#pragma once

#include <SDL.h>
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#include "Context_Platform.h"


namespace re
{
	class Context;
}


namespace opengl
{
	class Context
	{
	public:
		struct PlatformParams : public virtual platform::Context::PlatformParams
		{
			PlatformParams() = default;
			~PlatformParams() override = default;

			SDL_Window* m_glWindow = 0;
			SDL_GLContext m_glContext = 0;
		};

	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);

		static void SwapWindow(re::Context& context);
	};
}