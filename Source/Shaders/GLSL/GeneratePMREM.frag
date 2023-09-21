// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT
#define VOUT_LOCAL_POS

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


// Remap from equirectangular to cubemap, performing PMREM filtering (ie. for specular IBL)
void main()
{
	vec3 N = normalize(vOut.localPos);    
	vec3 R = N;
	vec3 V = R;		

	float totalWeight = 0.0;
	vec3 sampledColor = vec3(0.0);
	const int numSamples = int(g_numSamplesRoughnessFaceIdx.y);
	const float roughness = g_numSamplesRoughnessFaceIdx.z;
	for(int i = 0; i < numSamples; i++)
	{
		vec2 Xi = Hammersley2D(i, numSamples);
		vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
		vec3 L  = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0);
		if(NdotL > 0.0)
		{
			vec2 equirectangularUVs	= WorldDirToSphericalUV(L);

			sampledColor += texture(Tex0, equirectangularUVs).rgb;

			totalWeight += NdotL;
		}
	}
	sampledColor = sampledColor / totalWeight;

	FragColor = vec4(sampledColor, 1.0);
}