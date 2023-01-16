#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


uniform float exposure = 1.0;	// Uploaded in PostFXManager.Initialize()

void main()
{	
	// NOTE: uv0.y was flipped in toneMapShader.vert to account for SaberEngine's use of a (0,0) top-left uv convention
	vec4 color = texture(GBufferAlbedo, vOut.uv0.xy);

	vec3 toneMappedColor = vec3(1.0, 1.0, 1.0) - exp(-color.rgb * exposure);

	// Apply Gamma correction:
	toneMappedColor = Gamma(toneMappedColor);

	FragColor = vec4(toneMappedColor, 1.0);
} 