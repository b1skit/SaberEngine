// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class RenderManager;
	class Shader;
}

namespace platform
{
	class RenderManager
	{
	public:
		static void (*Initialize)(re::RenderManager&);
		static void (*Render)(re::RenderManager&);
		static void (*RenderImGui)(re::RenderManager&);
		static void (*Shutdown)(re::RenderManager&);

		static void (*CreateAPIResources)(re::RenderManager&);
	};
}