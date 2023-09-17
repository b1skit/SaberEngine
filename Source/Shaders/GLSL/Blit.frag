#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{	
	FragColor = texture(Tex0, vOut.uv0.xy);
} 