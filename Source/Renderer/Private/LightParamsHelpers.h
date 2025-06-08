// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "LightRenderData.h"
#include "ShadowMapRenderData.h"

struct AmbientLightData;
struct LightData;
struct PoissonSampleParamsData;
struct ShadowData;


namespace core
{
	template<typename T>
	class InvPtr;
}
namespace re
{
	class Texture;
}

namespace gr
{
	class RenderDataManager;

	AmbientLightData GetAmbientLightData(
		uint32_t numPMREMMips,
		float diffuseScale, 
		float specularScale, 
		const uint32_t dfgTexWidthHeight, 
		core::InvPtr<re::Texture> const& ssaoTex);

	LightData CreateDirectionalLightData(gr::Light::RenderDataDirectional const&, IDType, gr::RenderDataManager const&);
	LightData CreatePointLightData(gr::Light::RenderDataPoint const&, IDType, gr::RenderDataManager const&);
	LightData CreateSpotLightData(gr::Light::RenderDataSpot const&, IDType, gr::RenderDataManager const&);

	ShadowData CreateShadowData(gr::ShadowMap::RenderData const&, IDType, gr::RenderDataManager const&);

	PoissonSampleParamsData GetPoissonSampleParamsData();
}