#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{	
	// Debug: Override the skybox with a flat color
	if (g_backgroundColorIsEnabled.a == 1.f)
	{
		FragColor = g_backgroundColorIsEnabled;
		return;
	}

	const float sampleDepth = 0.f; // Arbitrary
	const vec3 worldPos = GetWorldPos(vOut.uv0, sampleDepth, g_invViewProjection);
	
	const vec3 sampleDir = worldPos - g_cameraWPos.xyz; // The skybox is centered about the camera

	const vec2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	FragColor = texture(Tex0, sphericalUVs);
}