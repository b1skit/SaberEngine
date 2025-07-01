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


	public: // re::RenderManager virtual interface:
		void Initialize() override;
		void Shutdown() override;
		void CreateAPIResources() override;
		void BeginFrame(uint64_t frameNum) override;
		void EndFrame() override;
		uint8_t GetNumFramesInFlight() override;


	private: // re::RenderManager interface:
		void Render() override;
	};

}