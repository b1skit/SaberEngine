// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"


namespace opengl
{
	class RenderManager final : public virtual re::RenderManager
	{
	public:
		RenderManager();
		~RenderManager() override = default;


	public: // Platform PIMPL:
		static void Initialize(re::RenderManager&);
		static void Shutdown(re::RenderManager&);
		static void CreateAPIResources(re::RenderManager&);
		static void EndOfFrame(re::RenderManager&);

		static uint8_t GetNumFramesInFlight(); // Number of frames in flight


	private: // re::RenderManager interface:
		void Render() override;
	};


	inline uint8_t RenderManager::GetNumFramesInFlight()
	{
		constexpr uint8_t k_numFrames = 2; // OpenGL only supports double buffering via a front and back buffer
		return k_numFrames;
	}
}