// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"


namespace dx12
{
	class RenderManager final : public virtual re::RenderManager
	{
	public:
		RenderManager();
		~RenderManager() override = default;

		static uint8_t s_numFrames_dx12;


	public:
		static uint8_t GetFrameOffsetIdx(re::RenderManager const& renderMgr); // Get an index in [0, NumFramesInFight)


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
		return s_numFrames_dx12;
	}


	inline uint8_t RenderManager::GetFrameOffsetIdx(re::RenderManager const& renderMgr)
	{
		return renderMgr.GetCurrentRenderFrameNum() % GetNumFramesInFlight();
	}
}