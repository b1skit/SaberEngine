// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "RayTracingCommon.hlsli"
#include "GBufferBindless.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/ResourceCommon.h"


// .x = SceneBVH idx, .y = TraceRayParams idx, .z = DescriptorIndexes, .w = unused
ConstantBuffer<RootConstantData> RootConstants0 : register(b0, space0);


[shader("closesthit")]
void ClosestHit_Experimental(inout ExperimentalPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
//#define TEST_VERTEX_STREAMS
#define TEST_MATERIALS
	
	const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);
	
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const ConstantBuffer<DescriptorIndexData> descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	// Get our Vertex stream LUTs buffer:
	const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes0.x;
	const StructuredBuffer<VertexStreamLUTData> vertexStreamLUT = VertexStreamLUTs[vertexStreamsLUTIdx];	
	
	// Compute our geometry index for buffer arrays aligned with AS geometry:
	const uint geoIdx = InstanceID() + GeometryIndex();
		
	const uint3 vertexIndexes = GetVertexIndexes(vertexStreamsLUTIdx, geoIdx);
	
#if defined(TEST_MATERIALS)
	
	const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes0.y;
	const StructuredBuffer<InstancedBufferLUTData> instancedBuffersLUT = InstancedBufferLUTs[instancedBufferLUTIdx];
	
	const uint materialResourceIdx = instancedBuffersLUT[geoIdx].g_materialIndexes.x;
	const uint materialBufferIdx = instancedBuffersLUT[geoIdx].g_materialIndexes.y;
	const uint materialType = instancedBuffersLUT[geoIdx].g_materialIndexes.z;
	
	uint baseColorResourceIdx = INVALID_RESOURCE_IDX;
	uint baseColorUVStreamResourceIdx = INVALID_RESOURCE_IDX;
	
	uint baseColorUVChannel = 0;
	float4 baseColorFactor = float4(1.f, 1.f, 1.f, 1.f);
	switch (materialType)
	{
	case MAT_ID_GLTF_Unlit:
	{
		const StructuredBuffer<UnlitData> materialData = UnlitParams[materialResourceIdx];
		baseColorResourceIdx = materialData[materialBufferIdx].g_bindlessTextureIndexes0.x;
			
		baseColorUVChannel = materialData[materialBufferIdx].g_uvChannelIndexes0.x;
			
		baseColorFactor = materialData[materialBufferIdx].g_baseColorFactor;

	}
	break;
	case MAT_ID_GLTF_PBRMetallicRoughness:
	{
		const StructuredBuffer<PBRMetallicRoughnessData> materialData = PBRMetallicRoughnessParams[materialResourceIdx];
		baseColorResourceIdx = materialData[materialBufferIdx].g_bindlessTextureIndexes0.x;
			
		baseColorUVChannel = materialData[materialBufferIdx].g_uvChannelIndexes0.x;
			
		baseColorFactor = materialData[materialBufferIdx].g_baseColorFactor;
	}
	break;
	}
	
	// UVs:
	baseColorUVStreamResourceIdx = baseColorUVChannel == 0 ? 
		vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w : vertexStreamLUT[geoIdx].g_UV1ColorIndex.x;
	
	const StructuredBuffer<float2> uvStream = VertexStreams_Float2[baseColorUVStreamResourceIdx];
	
	float2 uv =
		uvStream[vertexIndexes.x].xy * barycentrics.x +
		uvStream[vertexIndexes.y].xy * barycentrics.y +
		uvStream[vertexIndexes.z].xy * barycentrics.z;
	
	// Wrap the UVs (accounting for negative values, or values out of [0,1]):
	uv = uv - floor(uv);
	
	// Unpack the GBuffer:
	const GBuffer gbuffer = UnpackBindlessGBuffer(
		uv,
		baseColorResourceIdx,
		INVALID_RESOURCE_IDX, // worldNormalResourceIdx
		INVALID_RESOURCE_IDX, // RMAOVnResourceIdx
		INVALID_RESOURCE_IDX, // emissiveResourceIdx
		INVALID_RESOURCE_IDX, // matProp0ResourceIdx
		INVALID_RESOURCE_IDX, // materialIDResourceIdx
		INVALID_RESOURCE_IDX // depthResourceIdx
	);
	
	// Vertex color:
	float4 vertexColor = float4(1.f, 1.f, 1.f, 1.f);
	const uint colorVertexStreamIdx = vertexStreamLUT[geoIdx].g_UV1ColorIndex.y;
	if (colorVertexStreamIdx != INVALID_RESOURCE_IDX)
	{
		const StructuredBuffer<float4> vertexColorStream = VertexStreams_Float4[colorVertexStreamIdx];
		
		vertexColor = 
			vertexColorStream[vertexIndexes.x] * barycentrics.x +
			vertexColorStream[vertexIndexes.y] * barycentrics.y +
			vertexColorStream[vertexIndexes.z] * barycentrics.z;
	}
	
	// Combine:
	float3 colorOut = gbuffer.LinearAlbedo * vertexColor.rgb * baseColorFactor.rgb;
	
	
#elif defined(TEST_VERTEX_STREAMS)
	const uint colorStreamIdx = vertexStreamLUT[geoIdx].g_UV1ColorIndex.y;
	const StructuredBuffer<float4> colorStream = VertexStreams_Float4[colorStreamIdx];

	
#if defined(OPAQUE_SINGLE_SIDED)
	// Interpolate the vertex color:
	colorOut = 
		colorStream[vertexIndexes.x].rgb * barycentrics.x +
		colorStream[vertexIndexes.y].rgb * barycentrics.y +
		colorStream[vertexIndexes.z].rgb * barycentrics.z;
#elif defined(CLIP_SINGLE_SIDED)
	colorOut = float3(1,0,0);
#elif defined(OPAQUE_DOUBLE_SIDED)
	colorOut = float3(0,1,1);
#elif defined(CLIP_DOUBLE_SIDED)
	colorOut = float3(1,0,1);
#elif defined(BLEND_SINGLE_SIDED)
	colorOut = float3(1,1,0);
#elif defined(BLEND_DOUBLE_SIDED)
	colorOut = float3(1,1,1);
#endif // OPAQUE_SINGLE_SIDED

#endif // TEST_VERTEX_STREAMS
	
	payload.g_colorAndDistance = float4(colorOut, RayTCurrent());
}


[shader("anyhit")]
void AnyHit_Experimental(inout ExperimentalPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
	////float3 barycentrics =
	////  float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
	//const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);

	//uint vertId = 3 * PrimitiveIndex();
	//// #DXR Extra: Per-Instance Data
	//float3 hitColor = float3(0.6, 0.7, 0.6);
	//// Shade only the first 3 instances (triangles)
	//if (InstanceID() < 3)
	//{
	//	// #DXR Extra: Per-Instance Data
	//	hitColor = BTriVertex[indices[vertId + 0]].color.rgb * barycentrics.x +
	//		   BTriVertex[indices[vertId + 1]].color.rgb * barycentrics.y +
	//		   BTriVertex[indices[vertId + 2]].color.rgb * barycentrics.z;
	//}

	//payload.g_colorAndDistance = float4(hitColor, RayTCurrent());
	
	payload.g_colorAndDistance = float4(float3(1, 1, 0), RayTCurrent());
}


[shader("raygeneration")]
void RayGeneration_Experimental()
{
	// Initialize the ray payload
	ExperimentalPayload payload;
	payload.g_colorAndDistance = float4(0, 0, 0, 0);

	// Get the location within the dispatched 2D grid of work items
	// (often maps to pixels, so this could represent a pixel coordinate).
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);
	float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
	
	// Define a ray, consisting of origin, direction, and the min-max distance
	// values
	// #DXR Extra: Perspective Camera
	float aspectRatio = dims.x / dims.y;
	
	// Perspective
	RayDesc ray; // https://learn.microsoft.com/en-us/windows/win32/direct3d12/raydesc
	
	const uint sceneBVHDescriptorIdx = RootConstants0.g_data.x;
	const uint traceRayParamsIdx = RootConstants0.g_data.y;
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	
	const TraceRayData traceRayParams = TraceRayParams[traceRayParamsIdx];
	
	const ConstantBuffer<DescriptorIndexData> descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes0.z;
	const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	ray.Origin = mul(cameraParams.g_invView, float4(0, 0, 0, 1)).xyz;
	float4 target = mul(cameraParams.g_invProjection, float4(d.x, -d.y, 1, 1));
	ray.Direction = mul(cameraParams.g_invView, float4(target.xyz, 0)).xyz;
	
	ray.TMin = 0;
	ray.TMax = 100000;
	
	// Trace the ray
	TraceRay(
		// Parameter name: AccelerationStructure
		// Acceleration structure
		SceneBVH[sceneBVHDescriptorIdx],

		// Parameter name: RayFlags
		// Flags can be used to specify the behavior upon hitting a surface
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/ray_flag
		traceRayParams.g_rayFlags.x,

		// Parameter name: InstanceInclusionMask
		// Instance inclusion mask, which can be used to mask out some geometry to
		// this ray by and-ing the mask with a geometry mask. The 0xFF flag then
		// indicates no geometry will be masked
		traceRayParams.g_traceRayParams.x,

		// Parameter name: RayContributionToHitGroupIndex
		// Depending on the type of ray, a given object can have several hit
		// groups attached (ie. what to do when hitting to compute regular
		// shading, and what to do when hitting to compute shadows). Those hit
		// groups are specified sequentially in the SBT, so the value below
		// indicates which offset (on 4 bits) to apply to the hit groups for this
		// ray. In this sample we only have one hit group per object, hence an
		// offset of 0.
		traceRayParams.g_traceRayParams.y,

		// Parameter name: MultiplierForGeometryContributionToHitGroupIndex
		// The offsets in the SBT can be computed from the object ID, its instance
		// ID, but also simply by the order the objects have been pushed in the
		// acceleration structure. This allows the application to group shaders in
		// the SBT in the same order as they are added in the AS, in which case
		// the value below represents the stride (4 bits representing the number
		// of hit groups) between two consecutive objects.
		traceRayParams.g_traceRayParams.z,

		// Parameter name: MissShaderIndex
		// Index of the miss shader to use in case several consecutive miss
		// shaders are present in the SBT. This allows to change the behavior of
		// the program when no geometry have been hit, for example one to return a
		// sky color for regular rendering, and another returning a full
		// visibility value for shadow rays. This sample has only one miss shader,
		// hence an index 0
		traceRayParams.g_traceRayParams.w,

		// Parameter name: Ray
		// Ray information to trace
		ray,

		// Parameter name: Payload
		// Payload associated to the ray, which will be used to communicate
		// between the hit/miss shaders and the raygen
		payload);


	const uint gOutputDescriptorIdx = descriptorIndexes.g_descriptorIndexes0.w;
	RWTexture2D<float4> outputTex = Texture2DRWFloat4[gOutputDescriptorIdx];
	
#if defined(RAY_GEN_A)
	outputTex[launchIndex] = payload.g_colorAndDistance;
#endif
	
#if defined(RAY_GEN_B)
	const float scaleFactor = 0.5f;
	outputTex[launchIndex]  = payload.g_colorAndDistance * float4(scaleFactor, scaleFactor, scaleFactor, 1.f);
#endif
}


[shader("miss")]
void Miss_Experimental(inout ExperimentalPayload payload : SV_RayPayload)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);

	float ramp = launchIndex.y / dims.y;
	
#if defined(MISS_A)
	payload.g_colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
#elif defined(MISS_B)
	payload.g_colorAndDistance = float4(1.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
#else
	// Default case if neither MISS_A nor MISS_B is defined
	payload.g_colorAndDistance = float4(0.0f, 0.0f, 0.0f, -1.0f); // Default to black
#endif
}


[shader("callable")]
void Callable_Experimental(inout ExperimentalPayload payload : SV_RayPayload)
{
	//
}