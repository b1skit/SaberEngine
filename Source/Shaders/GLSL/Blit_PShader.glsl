#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsli"

layout(binding=0) uniform sampler2D Tex0;


void PShader()
{	
	FragColor = texture(Tex0, In.UV0.xy);
} 