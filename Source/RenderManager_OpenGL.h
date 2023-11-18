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

		static uint8_t GetNumFrames(); // Number of frames in flight

		static void StartImGuiFrame();
		static void RenderImGui();


	private: // re::RenderManager interface:
		void Render() override;


	private:
		static const uint8_t k_numFrames = 2; // OpenGL only supports double buffering via a front and back buffer
	};


	inline uint8_t RenderManager::GetNumFrames()
	{
		return k_numFrames;
	}
}