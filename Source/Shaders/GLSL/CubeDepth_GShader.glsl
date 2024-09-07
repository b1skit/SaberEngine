// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;


void GShader()
{
	for (int currentCubeFace = 0; currentCubeFace < 6; currentCubeFace++)
	{
		for (int currentVert = 0; currentVert < 3; currentVert++)
		{
			// Set the index of the cube map array & face we're rendering to
			// Note: gl_Layer becomes undefined after every EmitVertex() call; We must re-set it each iteration
			// https://www.khronos.org/opengl/wiki/Geometry_Shader#Layered_rendering
			gl_Layer = currentCubeFace;

			gl_Position = 
				_CubemapShadowRenderParams.g_cubemapShadowCam_VP[currentCubeFace] * gl_in[currentVert].gl_Position;

#if defined(SABER_INSTANCING)
			InstanceParamsOut.InstanceID = InstanceParamsIn[currentVert].InstanceID;

			// Not technically part of SABER_INSTANCING, but we only need this if we're executing a PShader
			Out.UV0 = In[currentVert].UV0;
#endif

			EmitVertex();
		}
		EndPrimitive();
	}
}