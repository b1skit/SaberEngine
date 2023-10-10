// © 2023 Adam Badke. All rights reserved.
#define SABER_DEPTH
#include "SaberCommon.glsl"


layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;
out vec4 FragPos;


void main()
{
	for (int currentCubeFace = 0; currentCubeFace < 6; currentCubeFace++)
	{
		gl_Layer = currentCubeFace; // Set the cube map face we're rendering to

		for (int currentVert = 0; currentVert < 3; currentVert++)
		{				
				FragPos = gl_in[currentVert].gl_Position; // World pos from vert shader

				gl_Position = g_cubemapShadowCam_VP[currentCubeFace] * gl_in[currentVert].gl_Position;
			
			EmitVertex();
		}    
		EndPrimitive();
	}
}