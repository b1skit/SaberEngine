// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderManager.h"

enum D3D_FEATURE_LEVEL;


namespace dx12
{
	class RenderManager : public virtual re::RenderManager
	{
	public:
		~RenderManager() override = default;

	public:
		static constexpr D3D_FEATURE_LEVEL GetTargetFeatureLevel();
		static constexpr uint8_t GetNumFrames(); // Number of frames in flight


	public: // Platform PIMPL:
		static void Initialize(re::RenderManager&);
		static void Shutdown(re::RenderManager&);

		static void StartImGuiFrame();
		static void RenderImGui();


	private: // re::RenderManager interface:
		void Render() override;
		void CreateAPIResources() override;


	private:
		static const D3D_FEATURE_LEVEL k_targetFeatureLevel;
		static const uint8_t k_numFrames = 3; // Number of frames in flight/number of backbuffers
	};


	inline constexpr D3D_FEATURE_LEVEL RenderManager::GetTargetFeatureLevel()
	{
		return k_targetFeatureLevel;
	}


	inline constexpr uint8_t RenderManager::GetNumFrames()
	{
		return k_numFrames;
	}
}

