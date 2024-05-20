// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsl"

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;


void main()
{
	for (int currentCubeFace = 0; currentCubeFace < 6; currentCubeFace++)
	{
		gl_Layer = currentCubeFace; // Set the cube map face we're rendering to

		for (int currentVert = 0; currentVert < 3; currentVert++)
		{
			gl_Position = 
				_CubemapShadowRenderParams.g_cubemapShadowCam_VP[currentCubeFace] * gl_in[currentVert].gl_Position;

#if defined(SABER_INSTANCING)
			InstanceParamsOut.InstanceID = InstanceParamsIn[currentVert].InstanceID;

			// Not technically part of SABER_INSTANCING, but we only need this if we're executing a PShader
			Out.uv0 = In[currentVert].uv0;
#endif

			EmitVertex();
		}
		EndPrimitive();
	}
}