// � 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"


namespace opengl
{
	class RenderManager final : public virtual re::RenderManager
	{
	public:
		RenderManager();
		~RenderManager() override = default;


	public: // Platform function overrides:
		void PlatformInitialize() override;
		void PlatformShutdown() override;
		void PlatformCreateAPIResources() override;
		void PlatformBeginFrame(uint64_t frameNum) override;
		void PlatformEndFrame() override;
		uint8_t PlatformGetNumFramesInFlight() const override;


	private: // re::RenderManager interface:
		void Render() override;
	};


	inline uint8_t RenderManager::PlatformGetNumFramesInFlight() const
	{
		constexpr uint8_t k_numFrames = 2; // OpenGL only supports double buffering via a front and back buffer
		return k_numFrames;
	}
}