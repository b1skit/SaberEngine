// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "ShadowMapRenderData.h"
#include "TransformRenderData.h"

struct AllLightIndexesData;
struct AmbientLightData;
struct LightData;
struct LightIndexData;
struct PoissonSampleParamsData;

namespace core
{
	template<typename T>
	class InvPtr;
}
namespace re
{
	class Texture;
	class TextureTargetSet;
}

namespace gr
{
	AmbientLightData GetAmbientLightData(
		uint32_t numPMREMMips,
		float diffuseScale, 
		float specularScale, 
		const uint32_t dfgTexWidthHeight, 
		core::InvPtr<re::Texture> const& ssaoTex);


	LightData GetLightData(
		void const* lightRenderData,
		gr::Light::Type lightType,
		gr::Transform::RenderData const& transformData,
		gr::ShadowMap::RenderData const* shadowData,
		gr::Camera::RenderData const* shadowCamData,
		core::InvPtr<re::Texture> const& shadowTex,
		uint32_t shadowArrayIdx);


	LightIndexData GetLightIndexData(uint32_t lightIndex, uint32_t shadowIndex);

	void PackAllLightIndexesDataValue(AllLightIndexesData&, gr::Light::Type, uint32_t lightIdx, uint32_t value);

	PoissonSampleParamsData GetPoissonSampleParamsData();
}