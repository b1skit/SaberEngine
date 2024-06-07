// © 2024 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "Transformations.glsl"
#include "UVUtils.glsl"


void main()
{	
	// Debug: Override the skybox with a flat color
	if (_SkyboxParams.g_backgroundColorIsEnabled.a == 1.f)
	{
		FragColor = _SkyboxParams.g_backgroundColorIsEnabled;
		return;
	}

	const float sampleDepth = 0.f; // Arbitrary
	const vec3 worldPos = ScreenUVToWorldPos(In.uv0, sampleDepth, _CameraParams.g_invViewProjection);
	
	const vec3 sampleDir = worldPos - _CameraParams.g_cameraWPos.xyz; // The skybox is centered about the camera

	const vec2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	FragColor = texture(Tex0, sphericalUVs, 0);
}