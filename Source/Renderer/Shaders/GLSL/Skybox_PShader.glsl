// � 2024 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsli"
#include "Transformations.glsli"
#include "UVUtils.glsli"

#include "../Common/CameraParams.h"
#include "../Common/SkyboxParams.h"

layout(binding=0) uniform SkyboxParams { SkyboxData _SkyboxParams; };
layout(binding=7) uniform CameraParams { CameraData _CameraParams; };

layout(binding=0) uniform sampler2D Tex0;


void PShader()
{	
	// Debug: Override the skybox with a flat color
	if (_SkyboxParams.g_backgroundColorIsEnabled.a == 1.f)
	{
		FragColor = _SkyboxParams.g_backgroundColorIsEnabled;
		return;
	}

	const float sampleDepth = 0.f; // Arbitrary
	const vec3 worldPos = ScreenUVToWorldPos(In.UV0, sampleDepth, _CameraParams.g_invViewProjection);
	
	const vec3 sampleDir = worldPos - _CameraParams.g_cameraWPos.xyz; // The skybox is centered about the camera

	const vec2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	FragColor = textureLod(Tex0, sphericalUVs, 0.0);
}