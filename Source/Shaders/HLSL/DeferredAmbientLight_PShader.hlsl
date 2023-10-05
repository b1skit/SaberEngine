// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "Transformations.hlsli"
#include "UVUtils.hlsli"


struct AmbientLightParamsCB
{
	// .x = max PMREM mip level, .y = pre-integrated DFG texture width/height, .z diffuse scale, .w = specular scale
	float4 g_maxPMREMMipDFGResScaleDiffuseScaleSpec;
};
ConstantBuffer<AmbientLightParamsCB> AmbientLightParams;


// Compute diffuse AO factor
// fineAO = AO from texture maps
float ComputeDiffuseAO(float fineAO)
{
	return fineAO;
}

// Compute the Frostbite specular AO factor
// Based on listing 26 (p.77) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// fineAO = AO from texture maps
float ComputeSpecularAO(float NoV, float remappedRoughness, float fineAO)
{
	const float totalAO = fineAO;
	return saturate(pow(NoV + totalAO, exp2(-16.f * remappedRoughness - 1.f)) - 1.f + fineAO);

}

// Compute a mip level for sampling the PMREM texture, using the remapped roughness
// Based on listing 63 (p.68) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// Note: maxMipLevel = Total number of mips - 1
float RemappedRoughnessToMipLevel(float remappedRoughness, float maxMipLevel)
{
	return sqrt(remappedRoughness) * maxMipLevel;
}

// Compute a mip level for sampling the PMREM texture, using the linear roughness
// Gives the same result as RemappedRoughnessToMipLevel
float LinearRoughnessToMipLevel(float linearRoughness, float maxMipLevel)
{
	return linearRoughness * maxMipLevel;
}


// Compute the diffuse color. For smooth, shiny metals we blend towards black as the specular contribution increases.
// Based on section B.3.5 "Metal BRDF and Dielectric BRDF" of the glTF 2.0 specifications
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf
float3 ComputeDiffuseColor(float3 linearAlbedo, float3 f0, float metalness)
{
	return linearAlbedo * (1.f - f0) * (1.f - metalness); // As per the GLTF specs
}


// Compute the blended Fresnel reflectance at incident angles (i.e L == N).
// The linearAlbedo defines the diffuse albedo for non-metallic surfaces, and the Fresnel reflectance at normal
// incidence for metallic surfaces. Thus, the linearMetalness value is used to blend between these.
// Based on section B.3.5 "Metal BRDF and Dielectric BRDF" of the glTF 2.0 specifications
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf
float3 ComputeBlendedF0(float3 f0, float3 linearAlbedo, float3 linearMetalness)
{
	return lerp(f0, linearAlbedo, linearMetalness);
}


// Image-based lights use luminance units, as per p.25 Moving Frostbite to Physically Based Rendering 3.0", 
// Lagarde et al.
// Lv = lm/m^2.cr = cd/m^2)
// Lv = Luminance, lm = Lumens, sr = steradians, cd = Candela
// Based on listing 24 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetDiffuseIBLContribution(float3 N, float3 V, float NoV, float remappedRoughness)
{
	static const float diffuseScale = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.z;
	
	const float3 dominantN = GetDiffuseDominantDir(N, V, NoV, remappedRoughness);
	
	const float3 diffuseLighting = CubeMap0.Sample(Wrap_Linear_Linear, WorldToCubeSampleDir(dominantN)).rgb; // IEM
	
	const float fDiffuse = Tex7.SampleLevel(Clamp_Linear_Linear, float2(NoV, remappedRoughness), 0).z;
	
	return diffuseLighting * fDiffuse * diffuseScale;
}


// Based on listing 24 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetSpecularIBLContribution(
	float3 N, float3 R, float3 V, float NoV, float linearRoughness, float remappedRoughness, float3 blendedF0)
{
	static const float maxPMREMMipLevel = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.x;
	static const float dfgTexWidthHeight = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.y;
	static const float specScale = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.w;

	const float3 dominantR = GetSpecularDominantDir(N, R, NoV, remappedRoughness);
	
	// Rebuild the function:
	// L * D * (f0 * GVis * (1.f - Fc) + GVis * Fc) * (cosTheta / (4.f * NoL * NoV))
	NoV = max(NoV, 0.5f / dfgTexWidthHeight);
	
	const float mipSampleLevel = LinearRoughnessToMipLevel(linearRoughness, maxPMREMMipLevel);

	const float3 H = ComputeNormalizedH(-dominantR, V);
	const float LoH = saturate(dot(dominantR, H));
	
	const float f90 = ComputeF90(remappedRoughness, LoH);
	
	const float3 preIntegratedLD =
		CubeMap1.SampleLevel(Wrap_LinearMipMapLinear_Linear, WorldToCubeSampleDir(dominantR), mipSampleLevel).rgb;
	
	// Sample the pre-integrated DFG texture
	//	Fc = (1.f - LoH)^5
	//	PreIntegratedDFG.r = GVis * (1.f - Fc)
	//	PreIntegratedDFG.g = GVis * Fc	
	const float2 preIntegratedDFG = Tex7.SampleLevel(Clamp_Linear_Linear, float2(NoV, remappedRoughness), 0).xy;
	
	// LD * (f0 * GVis * (1.f - Fc) + GVis * Fc * f90)
	return preIntegratedLD * (blendedF0 * preIntegratedDFG.r + blendedF0 * preIntegratedDFG.g) * specScale;
}


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.UV0);

	// Reconstruct the world position:
	const float4 worldPos = float4(GetWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection), 1.f);

	const float3 V = normalize(CameraParams.g_cameraWPos - worldPos.xyz); // point -> camera
	const float3 N = normalize(gbuffer.WorldNormal);
	
	const float NoV = saturate(dot(gbuffer.WorldNormal, V));
	
	const float linearRoughness = gbuffer.LinearRoughness;
	const float remappedRoughness = RemapRoughness(linearRoughness);
		
	const float3 diffuseContribution = GetDiffuseIBLContribution(N, V, NoV, remappedRoughness);
	const float diffuseAO = ComputeDiffuseAO(gbuffer.AO);
	
	const float3 dielectricSpecular = gbuffer.MatProp0.rgb;
	const float3 blendedF0 = ComputeBlendedF0(dielectricSpecular, gbuffer.LinearAlbedo, gbuffer.LinearMetalness);
	
	const float3 diffuseColor = ComputeDiffuseColor(gbuffer.LinearAlbedo, blendedF0, gbuffer.LinearMetalness);
	
	const float3 R = reflect(-V, N);
	
	const float3 specularContribution = GetSpecularIBLContribution(N, R, V, NoV, linearRoughness, remappedRoughness, blendedF0);
	const float specularAO = ComputeSpecularAO(NoV, remappedRoughness, gbuffer.AO);
	
	const float3 combinedContribution = 
		(diffuseColor * diffuseContribution * diffuseAO) + (specularContribution * specularAO);
	// Note: We're omitting the pi term in the albedo
	
	return float4(combinedContribution, 1.f);
}