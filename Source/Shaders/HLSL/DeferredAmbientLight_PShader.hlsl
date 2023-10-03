// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "GBufferCommon.hlsli"
#include "Sampling.hlsli"
#include "SaberCommon.hlsli"
#include "Transformations.hlsli"
#include "UVUtils.hlsli"


// Image-based lights use luminance units, as per p.25 Moving Frostbite to Physically Based Rendering 3.0", 
// Lagarde et al.
// Lv = lm/m^2.cr = cd/m^2)
// Lv = Luminance, lm = Lumens, sr = steradians, cd = Candela
float3 GetDiffuseIBLContribution(float3 N, float3 V, float NoV, float roughness)
{
	const float3 dominantN = GetDiffuseDominantDir(N, V, NoV, roughness);
	
	const float3 diffuseLighting = CubeMap0.Sample(Wrap_Linear_Linear, WorldToCubeSampleDir(dominantN)).rgb; // IEM
	
	const float DFG = Tex7.SampleLevel(Clamp_Linear_Linear, float2(NoV, roughness), 0).z;
	
	return diffuseLighting * DFG;
}


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.UV0);

	// Reconstruct the world position:
	const float4 worldPos = float4(GetWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection), 1.f);

	const float3 V = normalize(CameraParams.g_cameraWPos - worldPos.xyz);
	const float3 N = gbuffer.WorldNormal;
	
	const float NoV = saturate(dot(gbuffer.WorldNormal, V));
	
	const float roughness = gbuffer.Roughness; // TODO: Should this be remapped?
	
	const float3 diffuseContribution = GetDiffuseIBLContribution(N, V, NoV, roughness);
	
	const float3 combinedContribution = gbuffer.LinearAlbedo * diffuseContribution.rgb;
	
	return float4(combinedContribution, 1.f);
}