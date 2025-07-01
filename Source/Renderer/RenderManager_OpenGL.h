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


	public: // Platform-specific virtual interface implementation:
		void Initialize() override;
		void Shutdown() override;
		void CreateAPIResources() override;
		void BeginFrame(uint64_t frameNum) override;
		void EndFrame() override;
		uint8_t GetNumFramesInFlight() override;


	private: // re::RenderManager interface:
		void Render() override;
	};


	inline uint8_t RenderManager::GetNumFramesInFlight()
	{
		constexpr uint8_t k_numFrames = 2; // OpenGL only supports double buffering via a front and back buffer
		return k_numFrames;
	}
}