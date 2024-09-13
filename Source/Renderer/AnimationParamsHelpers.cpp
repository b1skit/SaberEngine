// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "AnimationParamsHelpers.h"

#include "Shaders/Common/AnimationParams.h"


namespace gr
{
	AnimationData GetAnimationParamsData(gr::MeshPrimitive::MeshRenderData const& animationRenderData)
	{
		AnimationData animationData{};

		memcpy(&animationData.g_morphWeights,
			animationRenderData.m_morphWeights.data(),
			sizeof(animationRenderData.m_morphWeights));

		return animationData;
	}
}