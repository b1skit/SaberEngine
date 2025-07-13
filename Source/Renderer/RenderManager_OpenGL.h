// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"


namespace opengl
{
	class RenderManager final : public virtual gr::RenderManager
	{
	public:
		RenderManager();
		~RenderManager() override = default;


	public: // Platform-specific virtual interface implementation:
		void Initialize_Platform() override;
		void Shutdown_Platform() override;
		void BeginFrame_Platform(uint64_t frameNum) override;
		void EndFrame_Platform() override;

		uint8_t GetNumFramesInFlight_Platform() const override;


	private: // gr::RenderManager interface:
		void Render() override;
	};


	inline uint8_t RenderManager::GetNumFramesInFlight_Platform() const
	{
		constexpr uint8_t k_numFramesInFlight = 2; // OpenGL only supports double buffering via a front and back buffer
		return k_numFramesInFlight;
	}
}