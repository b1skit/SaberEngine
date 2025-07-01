// � 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"


namespace dx12
{
	class RenderManager final : public virtual re::RenderManager
	{
	public:
		RenderManager();
		~RenderManager() override = default;


	public:
		static uint8_t GetFrameOffsetIdx(); // Get an index in [0, NumFramesInFight)


	public: // re::RenderManager virtual interface:
		void Initialize() override;
		void Shutdown() override;
		void CreateAPIResources() override;
		void BeginFrame(uint64_t frameNum) override;
		void EndFrame() override;
		uint8_t GetNumFramesInFlight() override;


	private: // re::RenderManager interface:
		void Render() override;
		

	private:
		const uint8_t m_numFrames;
	};


	inline uint8_t RenderManager::GetFrameOffsetIdx()
	{
		re::RenderManager const* renderMgr = re::RenderManager::Get();
		return renderMgr->GetCurrentRenderFrameNum() % static_cast<dx12::RenderManager const*>(renderMgr)->m_numFrames;
	}
}