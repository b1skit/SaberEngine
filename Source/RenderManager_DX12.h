// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderManager.h"


namespace dx12
{
	class RenderManager
	{
	public:
		// TODO: Figure out why creation fails for D3D_FEATURE_LEVEL_12_2
		static const D3D_FEATURE_LEVEL k_targetFeatureLevel = D3D_FEATURE_LEVEL_12_1;
		static const uint8_t k_numFrames = 3; // Number of frames in flight/number of backbuffers
		

	public:
		static void Initialize(re::RenderManager&);
		static void Render(re::RenderManager&);
		static void RenderImGui(re::RenderManager&);
		static void Shutdown(re::RenderManager&);

		static void CreateAPIResources(re::RenderManager&);
	};
}

