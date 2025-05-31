// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "RayTracingCommon.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/ResourceCommon.h"


struct GlobalConstantsData
{
	// .x = SceneBVH idx, .y = TraceRayParams idx, .z = DescriptorIndexes, .w = unused
	uint4 g_indexes; 
};
ConstantBuffer<GlobalConstantsData> GlobalConstants : register(b0, space0);


[shader("closesthit")]
void ClosestHit_Experimental(inout HitInfo_Experimental payload, BuiltInTriangleIntersectionAttributes attrib)
{
//#define TEST_VERTEX_STREAMS
#define TEST_MATERIALS
	
	const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);
	
	uint vertId = 3 * PrimitiveIndex(); // Triangle index -> Vertex index
	
	const uint descriptorIndexesIdx = GlobalConstants.g_indexes.z;
	const ConstantBuffer<DescriptorIndexData> descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	// Get our Vertex stream LUTs buffer:
	const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes.x;
	const StructuredBuffer<VertexStreamLUTData> vertexStreamLUT = VertexStreamLUTs[vertexStreamsLUTIdx];	
	
	// Fetch our bindless resources:
	const uint geoIdx = InstanceIndex() + GeometryIndex();
		
	const uint3 vertexIndexes = GetVertexIndexes(vertexStreamsLUTIdx, geoIdx, vertId);
	
	float3 colorOut = float3(0, 0, 0);
	
#if defined(TEST_MATERIALS)
	
	const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes.y;
	const StructuredBuffer<InstancedBufferLUTData> instancedBuffersLUT = InstancedBufferLUTs[instancedBufferLUTIdx];
	
	const uint materialResourceIdx = instancedBuffersLUT[geoIdx].g_materialIndexes.x;
	const uint materialBufferIdx = instancedBuffersLUT[geoIdx].g_materialIndexes.y;
	const uint materialType = instancedBuffersLUT[geoIdx].g_materialIndexes.z;
	
	uint baseColorResourceIdx = INVALID_RESOURCE_IDX;
	uint baseColorUVStreamResourceIdx = INVALID_RESOURCE_IDX;
	
	uint baseColorUVChannel = 0;
	switch (materialType)
	{
	case MAT_ID_GLTF_Unlit:
	{
		const StructuredBuffer<UnlitData> materialData = UnlitParams[materialResourceIdx];
		baseColorResourceIdx = materialData[materialBufferIdx].g_bindlessTextureIndexes0.x;
			
		baseColorUVChannel = materialData[materialBufferIdx].g_uvChannelIndexes0.x;
	}
	break;
	case MAT_ID_GLTF_PBRMetallicRoughness:
	{
		const StructuredBuffer<PBRMetallicRoughnessData> materialData = PBRMetallicRoughnessParams[materialResourceIdx];
		baseColorResourceIdx = materialData[materialBufferIdx].g_bindlessTextureIndexes0.x;
			
		baseColorUVChannel = materialData[materialBufferIdx].g_uvChannelIndexes0.x;
	}
	break;
	}
	
	// UVs:
	baseColorUVStreamResourceIdx = baseColorUVChannel == 0 ? 
		vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w : vertexStreamLUT[geoIdx].g_UV1ColorIndex.x;
	
	const StructuredBuffer<float2> uvStream = VertexStreams_Float2[baseColorUVStreamResourceIdx];
	
	const float2 uv =
		uvStream[vertexIndexes.x].xy * barycentrics.x +
		uvStream[vertexIndexes.y].xy * barycentrics.y +
		uvStream[vertexIndexes.z].xy * barycentrics.z;
	
	if (baseColorResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> baseColorTex = Texture2DFloat4[baseColorResourceIdx];
	
		uint3 baseColorDimensions;
		baseColorTex.GetDimensions(0, baseColorDimensions.x, baseColorDimensions.y, baseColorDimensions.z);
	
		// Convert the UVs to pixel coordinates:
		const uint3 baseColorCoords = uint3(baseColorDimensions.xy * uv, 0);
	
		colorOut = baseColorTex.Load(baseColorCoords).rgb;
	}
	
	
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
	
	payload.colorAndDistance = float4(colorOut, RayTCurrent());
}


[shader("anyhit")]
void AnyHit_Experimental(inout HitInfo_Experimental payload, BuiltInTriangleIntersectionAttributes attrib)
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

	//payload.colorAndDistance = float4(hitColor, RayTCurrent());
	
	payload.colorAndDistance = float4(float3(1, 1, 0), RayTCurrent());
}


[shader("raygeneration")]
void RayGeneration_Experimental()
{
	// Initialize the ray payload
	HitInfo_Experimental payload;
	payload.colorAndDistance = float4(0, 0, 0, 0);

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
	
	const uint sceneBVHDescriptorIdx = GlobalConstants.g_indexes.x;
	const uint traceRayParamsIdx = GlobalConstants.g_indexes.y;
	const uint descriptorIndexesIdx = GlobalConstants.g_indexes.z;
	
	const TraceRayData traceRayParams = TraceRayParams[traceRayParamsIdx];
	
	const ConstantBuffer<DescriptorIndexData> descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes.z;
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

	// ---
	// Artificially use .w to attempt to address -Wpayload-access-perf
	// This is done with a tiny multiplier to minimize visual impact if .w contains large scene-dependent values (like distance)
	payload.colorAndDistance.r += payload.colorAndDistance.w * 0.0000001f;
	// ---
	
	const uint gOutputDescriptorIdx = descriptorIndexes.g_descriptorIndexes.w;
	RWTexture2D<float4> outputTex = Texture2DRWFloat4[gOutputDescriptorIdx];
	
#if defined(RAY_GEN_A)
	outputTex[launchIndex] = float4(payload.colorAndDistance.rgb, 1.f);
#endif
	
#if defined(RAY_GEN_B)
	const float scaleFactor = 0.5f;
	outputTex[launchIndex]  = float4(payload.colorAndDistance.rgb * scaleFactor, 1.f);
#endif
}


[shader("miss")]
void Miss_Experimental(inout HitInfo_Experimental payload : SV_RayPayload)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);

	float ramp = launchIndex.y / dims.y;
	
#if defined(MISS_BLUE)
	payload.colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
#elif defined(MISS_RED)
	payload.colorAndDistance = float4(1.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
#else
	// Default case if neither MISS_BLUE nor MISS_RED is defined
	payload.colorAndDistance = float4(0.0f, 0.0f, 0.0f, -1.0f); // Default to black
#endif
}


[shader("callable")]
void Callable_Experimental(inout HitInfo_Experimental payload : SV_RayPayload)
{
	//
}