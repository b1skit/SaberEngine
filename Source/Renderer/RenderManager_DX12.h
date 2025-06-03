// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"


namespace dx12
{
	class RenderManager final : public virtual re::RenderManager
	{
	public:
		RenderManager(platform::RenderingAPI api, uint8_t numFramesInFlight);
		~RenderManager() override = default;


	public:
		static uint8_t GetFrameOffsetIdx(); // Get an index in [0, NumFramesInFight)


	public: // Platform PIMPL:
		static void Initialize(re::RenderManager&);
		static void Shutdown(re::RenderManager&);
		static void CreateAPIResources(re::RenderManager&);
		static void BeginFrame(re::RenderManager&, uint64_t frameNum);
		static void EndFrame(re::RenderManager&);

		static uint8_t GetNumFramesInFlight(); // Number of frames in flight


	private: // re::RenderManager interface:
		void Render() override;
		

	private:
		const uint8_t m_numFrames;
	};


	inline uint8_t RenderManager::GetNumFramesInFlight()
	{
		return dynamic_cast<dx12::RenderManager*>(re::RenderManager::Get())->m_numFrames;
	}


	inline uint8_t RenderManager::GetFrameOffsetIdx()
	{
		re::RenderManager const* renderMgr = re::RenderManager::Get();
		return renderMgr->GetCurrentRenderFrameNum() % renderMgr->GetNumFramesInFlight();
	}
}