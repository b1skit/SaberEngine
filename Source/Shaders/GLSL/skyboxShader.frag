#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{	
	const vec2 screenUV = PixelCoordsToUV(gl_FragCoord.xy, g_skyboxTargetResolution.xy);

	const float sampleDepth = 0.f; // Arbitrary

	const vec3 worldPos = GetWorldPos(screenUV, sampleDepth, g_invViewProjection);
	
	const vec3 sampleDir = worldPos - g_cameraWPos.xyz; // The skybox is centered about the camera

	const vec2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	FragColor = texture(Tex0, sphericalUVs);
}