// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT
#define VOUT_COLOR
#define VOUT_TBN
#define VOUT_WORLD_POS
#define SABER_INSTANCING

#include "SaberCommon.glsl"

#include "AmbientCommon.glsl"
#include "NormalMapUtils.glsl"


void main()
{
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[InstanceParamsIn.InstanceID].g_materialIdx;

	AmbientLightingParams lightingParams;
	
	const vec3 worldPos = In.WorldPos;
	lightingParams.WorldPosition = worldPos;
	
	lightingParams.V = normalize(_CameraParams.g_cameraWPos.xyz - worldPos); // point -> camera
	
	const float normalScaleFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.z;
	const vec3 normalScale = vec3(normalScaleFactor, normalScaleFactor, 1.f);
	const vec3 texNormal = texture(MatNormal, In.uv0.xy).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, In.TBN) * normalScale;
	lightingParams.WorldNormal = worldNormal;
	
	const float4 matAlbedo = texture(MatAlbedo, In.uv0.xy);
	const float4 baseColorFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor;
	lightingParams.LinearAlbedo = (matAlbedo * baseColorFactor * In.Color).rgb;
	
	lightingParams.DielectricSpecular = 
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_f0.rgb;
	
	const float linearRoughnessFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.y;
	
	const float metallicFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.x;
	
	const vec2 roughnessMetalness =
		texture(MatMetallicRoughness, In.uv0.xy).gb * vec2(linearRoughnessFactor, metallicFactor);
	lightingParams.LinearMetalness = roughnessMetalness.y;
	
	lightingParams.LinearRoughness = roughnessMetalness.x;
	lightingParams.RemappedRoughness = RemapRoughness(roughnessMetalness.x);

	const float occlusionStrength =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.w;
	const float occlusion = texture(MatOcclusion, In.uv0.xy).r * occlusionStrength;
	
	lightingParams.FineAO = occlusion;
	lightingParams.CoarseAO = 1.f; // No SSAO for transparents
	
	FragColor = vec4(ComputeAmbientLighting(lightingParams), matAlbedo.a);
}