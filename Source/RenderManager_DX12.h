// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderManager.h"


namespace dx12
{
	class RenderManager
	{
	public:
		static void Initialize(re::RenderManager&);
		static void Render(re::RenderManager&);
		static void RenderImGui(re::RenderManager&);
		static void Shutdown(re::RenderManager&);
	};
}

