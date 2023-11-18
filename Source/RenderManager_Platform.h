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
	public: // Platform PIMPL:
		static void (*Initialize)(re::RenderManager&);
		static void (*Shutdown)(re::RenderManager&);
		static void (*CreateAPIResources)(re::RenderManager&);

		static uint8_t (*GetNumFrames)();

		// ImgGui:
		static void (*StartImGuiFrame)();
		static void (*RenderImGui)();
	};
}