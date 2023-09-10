// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderManager.h"

namespace opengl
{
	class RenderManager : public virtual re::RenderManager
	{
	public:
		~RenderManager() override = default;


	public: // Platform PIMPL:
		static void Initialize(re::RenderManager&);
		static void Shutdown(re::RenderManager&);
		static void CreateAPIResources(re::RenderManager&);

		static void StartImGuiFrame();
		static void RenderImGui();


	private: // re::RenderManager interface:
		void Render() override;
	};
}