// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "Random.hlsli"
#include "RayTracingCommon.hlsli"
#include "Sampling.hlsli"
#include "UVUtils.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/RTAOParams.h"
#include "../Common/ResourceCommon.h"


// .x = SceneBVH idx, .y = TraceRayParams idx, .z = DescriptorIndexes, .w = unused
ConstantBuffer<RootConstantData> RootConstants0 : register(b0, space0);


[shader("raygeneration")]
void RTAO_RayGeneration()
{	
	// Unpack root constant indices:
    const uint sceneBVHDescriptorIdx = RootConstants0.g_data.x;
    const uint traceRayParamsIdx = RootConstants0.g_data.y;
    const uint descriptorIndexesIdx = RootConstants0.g_data.z;
    const uint rtaoParamsIdx = RootConstants0.g_data.w;
	
	// Get resources from our bindless arrays:
    RaytracingAccelerationStructure sceneBVH = SceneBVH[sceneBVHDescriptorIdx];
    const TraceRayData traceRayParams = TraceRayParams[traceRayParamsIdx];
    const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
    const RTAOParamsData rtaoParams = RTAOParams[rtaoParamsIdx];
	
    const uint gbufferDepthIdx = rtaoParams.g_indexes.x;
    const uint gbufferNormalIdx = rtaoParams.g_indexes.y;	
	
    const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes0.z;
    const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	// Convert the launch pixel coords to UV coordinates:
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 dims = DispatchRaysDimensions().xy;
    const float2 uvs = PixelCoordsToScreenUV(launchIndex, dims, float2(0.5f, 0.5f));
	
	const uint3 loadCoords = uint3(launchIndex.xy, 0);
	
	// Get the GBuffer depth:    
    Texture2D<float> gbufferDepthTex = Texture2DFloat[gbufferDepthIdx];
	const float nonLinearDepth = gbufferDepthTex.Load(loadCoords);
	
	// Get the world position:
    const float3 worldPos = ScreenUVToWorldPos(uvs, nonLinearDepth, cameraParams.g_invViewProjection);
	
	// Get the GBuffer normal:
    Texture2D<float4> gbufferNormalTex = Texture2DFloat4[gbufferNormalIdx];
	const float3 worldNormal = gbufferNormalTex.Load(loadCoords).xyz;
	
	// Get our output target:
	const uint outputDescriptorIdx = descriptorIndexes.g_descriptorIndexes0.w;
	RWTexture2D<float> outputTex = Texture2DRWFloat[outputDescriptorIdx];
	
	const uint numRays = rtaoParams.g_params.z;
	const bool isEnabled = rtaoParams.g_params.w > 0.5f;
	
	// If we have no rays, return early:	
	if (numRays == 0 || isEnabled == false)
	{
		outputTex[launchIndex].r = 1.f;
		return;
	}
	
	const Referential localReferential = BuildReferential(worldNormal, float3(0, 1, 0));
		
	// Note: We ignore the frame index as we don't temporally accumulate
	RNGState1D sampleGen = InitializeRNGState1D(launchIndex.xy, 0.f); 
	
	float visibility = 0.f;
	for (uint i = 0; i < numRays; i++)
	{
		const float angularOffset = GetNextFloat(sampleGen);
		
		float3 sampleDir;
		float NoL;
		float pdf;
		ImportanceSampleFibonacciSpiralDir(i, numRays, angularOffset, localReferential, sampleDir, NoL, pdf);
		
		// Build our AO rays:
		RayDesc ray; // https://learn.microsoft.com/en-us/windows/win32/direct3d12/raydesc
		ray.Origin = worldPos;
		ray.TMin = rtaoParams.g_params.x;
		ray.TMax = rtaoParams.g_params.y;
		ray.Direction = normalize(sampleDir);
	
		// Initialize the ray hitInfo:
		VisibilityPayload hitInfo;
		hitInfo.g_visibility = 0.f;
		
		// Trace the ray
		TraceRay(
			sceneBVH, // Acceleration structure
			traceRayParams.g_rayFlags.x, // RayFlags
			traceRayParams.g_traceRayParams.x, // InstanceInclusionMask
			traceRayParams.g_traceRayParams.y, // RayContributionToHitGroupIndex
			traceRayParams.g_traceRayParams.z, // MultiplierForGeometryContributionToHitGroupIndex
			traceRayParams.g_traceRayParams.w, // MissShaderIndex
			ray, // Ray to trace
			hitInfo); // Payload
		
		visibility += hitInfo.g_visibility;
	}
	
	const float visibilityFactor = visibility / numRays;

	outputTex[launchIndex].r = visibilityFactor;
}


[shader("anyhit")]
void RTAO_AnyHit(inout VisibilityPayload hitInfo, BuiltInTriangleIntersectionAttributes attrib)
{
#if defined(ALPHA_CLIP) || defined(ALPHA_BLEND)
	
	const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);
	
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	// Get our Vertex stream LUTs buffer:
	const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes0.x;
	const StructuredBuffer<VertexStreamLUTData> vertexStreamLUT = VertexStreamLUTs[vertexStreamsLUTIdx];
	
	// Compute our geometry index for buffer arrays aligned with AS geometry:
	const uint geoIdx = InstanceID() + GeometryIndex();
	
	const uint3 vertexIndexes = GetVertexIndexes(vertexStreamsLUTIdx, geoIdx);
	
	const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes0.y;
	const InstancedBufferLUTData instancedBuffersLUT = InstancedBufferLUTs[instancedBufferLUTIdx][geoIdx];
	
	const uint materialResourceIdx = instancedBuffersLUT.g_materialIndexes.x;
	const uint materialBufferIdx = instancedBuffersLUT.g_materialIndexes.y;
	const uint materialType = instancedBuffersLUT.g_materialIndexes.z;
	
	float alphaCutoff = 0.f;
	uint baseColorResourceIdx = INVALID_RESOURCE_IDX;
	uint baseColorUVChannel = 0;
	switch (materialType)
	{
	case MAT_ID_GLTF_Unlit:
	{
		const UnlitData materialBuffer = UnlitParams[materialResourceIdx][materialBufferIdx];

		baseColorResourceIdx = materialBuffer.g_bindlessTextureIndexes0.x;
		baseColorUVChannel = materialBuffer.g_uvChannelIndexes0.x;
		alphaCutoff = materialBuffer.g_alphaCutuff.x;
	}
	break;
	case MAT_ID_GLTF_PBRMetallicRoughness:
	{
		const PBRMetallicRoughnessData materialBuffer = PBRMetallicRoughnessParams[materialResourceIdx][materialBufferIdx];
			
		baseColorResourceIdx = materialBuffer.g_bindlessTextureIndexes0.x;
		baseColorUVChannel = materialBuffer.g_uvChannelIndexes0.x;
		alphaCutoff = materialBuffer.g_f0AlphaCutoff.w;
	}
	break;
	}
	
	// UVs:
	const uint baseColorUVStreamResourceIdx = baseColorUVChannel == 0 ?
		vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w : vertexStreamLUT[geoIdx].g_UV1ColorIndex.x;
	
	const StructuredBuffer<float2> uvStream = VertexStreams_Float2[baseColorUVStreamResourceIdx];
	
	float2 uv =
		uvStream[vertexIndexes.x].xy * barycentrics.x +
		uvStream[vertexIndexes.y].xy * barycentrics.y +
		uvStream[vertexIndexes.z].xy * barycentrics.z;
	
	// Wrap the UVs (accounting for negative values, or values out of [0,1]):
	uv = uv - floor(uv);
	
	const float alpha = Texture2DFloat4[baseColorResourceIdx].SampleLevel(WrapMinMagMipLinear, uv, 0);
	
	hitInfo.g_visibility = alpha; // Increase visibility when no geometry is hit
	
#else
	hitInfo.g_visibility = 0.f; // Only increment visibility when no geometry is hit
#endif
}


[shader("miss")]
void RTAO_Miss(inout VisibilityPayload hitInfo : SV_RayPayload)
{
	hitInfo.g_visibility = 1.f; // Increment visibility when no geometry is hit
}