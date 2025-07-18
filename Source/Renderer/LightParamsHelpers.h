// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "LightRenderData.h"
#include "RenderObjectIDs.h"
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
namespace gr
{
	class RenderDataManager;
}
namespace re
{
	class Texture;
}

namespace grutil
{
	class RenderDataManager;

	AmbientLightData GetAmbientLightData(
		uint32_t numPMREMMips,
		float diffuseScale, 
		float specularScale, 
		const uint32_t dfgTexWidthHeight, 
		core::InvPtr<re::Texture> const& AOTex);

	LightData CreateDirectionalLightData(gr::Light::RenderDataDirectional const&, gr::IDType, gr::RenderDataManager const&);
	LightData CreatePointLightData(gr::Light::RenderDataPoint const&, gr::IDType, gr::RenderDataManager const&);
	LightData CreateSpotLightData(gr::Light::RenderDataSpot const&, gr::IDType, gr::RenderDataManager const&);

	ShadowData CreateShadowData(gr::ShadowMap::RenderData const&, gr::IDType, gr::RenderDataManager const&);

	PoissonSampleParamsData GetPoissonSampleParamsData();
}