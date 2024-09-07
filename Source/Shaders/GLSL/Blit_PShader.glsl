#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsli"


void PShader()
{	
	FragColor = texture(Tex0, In.UV0.xy);
} 