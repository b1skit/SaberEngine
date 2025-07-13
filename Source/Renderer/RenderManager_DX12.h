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


	private: // Platform-specific virtual interface implementation:
		void Initialize_Platform() override;
		void Shutdown_Platform() override;
		void BeginFrame_Platform(uint64_t frameNum) override;
		void EndFrame_Platform() override;

		uint8_t GetNumFramesInFlight_Platform() const override;


	private: // gr::RenderManager interface:
		void Render() override;
		

	private:
		const uint8_t m_numFramesInFlight;
	};


	inline uint8_t RenderManager::GetNumFramesInFlight_Platform() const
	{
		return m_numFramesInFlight;
	}
}