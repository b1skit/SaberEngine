#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"


void main()
{	
	FragColor = texture(Tex0, In.UV0.xy);
} 