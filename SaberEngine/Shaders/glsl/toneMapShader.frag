#version 430 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


uniform float exposure = 1.0;	// Uploaded in PostFXManager.Initialize()

void main()
{	
	vec4 color = texture(GBuffer_Albedo, data.uv0.xy);

	vec3 toneMappedColor = vec3(1.0, 1.0, 1.0) - exp(-color.rgb * exposure);

	// Apply Gamma correction:
	toneMappedColor = Gamma(toneMappedColor);

	FragColor = vec4(toneMappedColor, 1.0);
} 