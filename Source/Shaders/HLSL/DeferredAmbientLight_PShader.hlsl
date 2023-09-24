// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "Transformations.hlsli"


// TODO: Define the units returned by this
float3 GetDiffuseIBLContribution(float3 N, float3 V, float NoV, float roughness)
{
	const float3 dominantN = GetDiffuseDominantDir(N, V, NoV, roughness);
	
	const float3 diffuseLighting = CubeMap0.Sample(Wrap_Linear_Linear, dominantN).rgb;
	
	//// NEED TO UPDATE DFG INTEGRATION SHADER
	//const float DFG = Tex7.Sample(Clamp_Linear_Linear, float2(NoV, gbuffer.Roughness)).z;
	
	//return diffuseLighting * DFG;
	
	return diffuseLighting;
}


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.UV0);
	
	// Reconstruct the world position:
	const float4 worldPos = float4(GetWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection), 1.f);
	
	return float4(worldPos);

	//const float4 viewPosition = mul(CameraParams.g_view, worldPos);
	//const float3 viewEyeDir = normalize(-viewPosition.xyz); // View-space eye/camera direction

	//const float3x3 viewRotationScale = float3x3(
	//	CameraParams.g_view[0].xyz,
	//	CameraParams.g_view[1].xyz,
	//	CameraParams.g_view[2].xyz
	//);

	//const float3 viewNormal = normalize(mul(viewRotationScale, gbuffer.WorldNormal)); // View-space surface normal

	//const float NoV = saturate(dot(viewNormal, viewEyeDir));
	
	//// TODO: should we be lighting in world-space, or view-space?
	//const float3 iblDiffuse = GetDiffuseIBLContribution(viewNormal, viewEyeDir, NoV, gbuffer.Roughness);
	

	
	//return float4(gbuffer.LinearAlbedo.rgb * iblDiffuse * gbuffer.AO, 1.f); // TEMP HAX
	
	
	

//	float3 F0 = gbuffer.MatProp0.rgb; // .rgb = F0 (Surface response at 0 degrees)
//	F0 = lerp(F0, gbuffer.LinearAlbedo, gbuffer.Metalness);
//	// Linear interpolation: x, y, using t=[0,1]. Returns x when t=0 -> Blends towards MatAlbedo for metals

////	float3 fresnel_kS = FresnelSchlick(NoV, F0); // Doesn't quite look right: Use FresnelSchlick_Roughness() instead
//	const float3 fresnel_kS = FresnelSchlick_Roughness(NoV, F0, gbuffer.Roughness);
//	const float3 k_d = 1.0 - fresnel_kS;

//	// Sample the diffuse irradiance from our prefiltered irradiance environment map.
//	// Note: We must flip the Y component of our normal to compensate for our UV (0,0) top-left convention
//	const float3 diffuseCubeSampleDir = float3(gbuffer.WorldNormal.x, -gbuffer.WorldNormal.y, gbuffer.WorldNormal.z);
//	const float3 irradiance = texture(CubeMap0, diffuseCubeSampleDir).xyz * gbuffer.AO;

//	// Get the specular reflectance term:
//	const float3 worldView = normalize(g_cameraWPos - worldPos.xyz); // Direction = Point -> Eye
//	float3 worldReflection = normalize(reflect(-worldView, gbuffer.WorldNormal));
//	worldReflection.y *= -1; // Note: We flip Y here to compensate for our UV (0,0) top-left convention

//	const float roughness = gbuffer.Roughness;
//	const float remappedRoughness = RemapRoughnessIBL(roughness);

//	// Sample our generated BRDF Integration map using the non-remapped roughness
//	const float2 BRDF = texture(Tex7, float2(max(NoV, 0.0), roughness)).rg;

//	const float3 specular =
//		textureLod(CubeMap1, worldReflection, remappedRoughness * g_maxPMREMMip).xyz * ((fresnel_kS * BRDF.x) + BRDF.y);

//	const float3 combinedContribution = (gbuffer.LinearAlbedo * irradiance * k_d + specular); // Note: Omitted the "/ PI" factor here

//	// Apply exposure:
//	const float3 exposedColor = ApplyExposure(combinedContribution, g_exposureProperties.x);
	
//	return float4(exposedColor, 1.0); // Note: Omitted the "/ PI" factor here

}