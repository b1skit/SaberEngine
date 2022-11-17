#pragma once


namespace re
{
	class RenderManager;
}

namespace opengl
{
	class RenderManager
	{
	public:
		static void Initialize(re::RenderManager&);
		static void StartOfFrame();
		static void Render(re::RenderManager&);		
	};
}