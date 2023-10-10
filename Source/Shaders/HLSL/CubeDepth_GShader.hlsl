// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"


struct GeometryOut
{
	float4 Position : SV_Position;
	uint Face		: SV_RenderTargetArrayIndex;
};


struct CubemapShadowRenderParamsCB
{
	float4x4 g_cubemapShadowCam_VP[6];
	float4 g_cubemapShadowCamNearFar; // .xy = near, far. .zw = unused
	float4 g_cubemapLightWorldPos; // .xyz = light word pos, .w = unused
};
ConstantBuffer<CubemapShadowRenderParamsCB> CubemapShadowRenderParams;


[maxvertexcount(18)]
void GShader(triangle VertexToGeometry In[3], inout TriangleStream<GeometryOut> StreamOut)
{
	for (uint faceIdx = 0; faceIdx < 6; faceIdx++)
	{
		GeometryOut Out;
		Out.Face = faceIdx;
		
		for (uint vertIdx = 0; vertIdx < 3; vertIdx++)
		{
			Out.Position = mul(CubemapShadowRenderParams.g_cubemapShadowCam_VP[faceIdx], In[vertIdx].Position);	
			
			StreamOut.Append(Out);
		}
		
		StreamOut.RestartStrip();
	}
}