// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"

#include "../Common/ShadowParams.h"


struct GeometryOut
{
	float4 Position : SV_Position;
	
#ifdef VOUT_UV0
	float2 UV0		: TEXCOORD0;
#endif
#ifdef SABER_INSTANCING
	nointerpolation uint InstanceID : SV_InstanceID;
#endif
	
	uint Face : SV_RenderTargetArrayIndex;
};

ConstantBuffer<CubeShadowRenderData> CubeShadowRenderParams;


[maxvertexcount(18)]
void GShader(triangle VertexOut In[3], inout TriangleStream<GeometryOut> StreamOut)
{
	for (uint faceIdx = 0; faceIdx < 6; faceIdx++)
	{
		GeometryOut Out;
		Out.Face = faceIdx;
		
		for (uint vertIdx = 0; vertIdx < 3; vertIdx++)
		{
			Out.Position = mul(CubeShadowRenderParams.g_cubemapShadowCam_VP[faceIdx], In[vertIdx].Position);
			
#if defined(VOUT_UV0)
			Out.UV0 = In[vertIdx].UV0;
#endif
#if defined(SABER_INSTANCING)
			Out.InstanceID = In[vertIdx].InstanceID;
#endif
			
			StreamOut.Append(Out);
		}
		
		StreamOut.RestartStrip();
	}
}