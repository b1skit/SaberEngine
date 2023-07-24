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


	private: // re::RenderManager interface:
		void Render() override;
		void RenderImGui() override;
		void CreateAPIResources() override;
	};
}