// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"


namespace dx12
{
	class RenderManager final : public virtual gr::RenderManager
	{
	public:
		RenderManager();
		~RenderManager() override = default;


	public:
		static uint8_t GetFrameOffsetIdx(); // Get an index in [0, NumFramesInFight)


	public: // Platform-specific virtual interface implementation:
		void Initialize_Platform() override;
		void Shutdown_Platform() override;
		void CreateAPIResources_Platform() override;
		void BeginFrame_Platform(uint64_t frameNum) override;
		void EndFrame_Platform() override;

		uint8_t GetNumFramesInFlight() const override;


	private: // gr::RenderManager interface:
		void Render() override;
		

	private:
		const uint8_t m_numFrames;
	};


	inline uint8_t RenderManager::GetNumFramesInFlight() const
	{
		return m_numFrames;
	}


	inline uint8_t RenderManager::GetFrameOffsetIdx()
	{
		dx12::RenderManager const* renderMgr = dynamic_cast<dx12::RenderManager const*>(gr::RenderManager::Get());
		return renderMgr->GetCurrentRenderFrameNum() % renderMgr->GetNumFramesInFlight();
	}
}