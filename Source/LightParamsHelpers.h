// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "LightRenderData.h"
#include "ShadowMapRenderData.h"
#include "TransformRenderData.h"

struct AmbientLightData;
struct LightData;
struct LightIndexData;
struct PoissonSampleParamsData;

namespace re
{
	class Texture;
	class TextureTargetSet;
}

namespace gr
{
	AmbientLightData GetAmbientLightParamsData(
		uint32_t numPMREMMips,
		float diffuseScale, 
		float specularScale, 
		const uint32_t dfgTexWidthHeight, 
		re::Texture const* ssaoTex);


	LightData GetLightParamData(
		void const* lightRenderData,
		gr::Light::Type lightType,
		gr::Transform::RenderData const& transformData,
		gr::ShadowMap::RenderData const* shadowData,
		gr::Camera::RenderData const* shadowCamData,
		re::Texture const* shadowTex);


	LightIndexData GetLightIndexData(uint32_t lightIndex, uint32_t shadowIndex);


	PoissonSampleParamsData GetPoissonSampleParamsData();
}