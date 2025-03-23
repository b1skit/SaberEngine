// © 2025 Adam Badke. All rights reserved.
#include "../Common/BindlessResourceParams.h"


// We use register spaces to overlap unbounded arrays on register 0
StructuredBuffer<float3> VertexStreams_Position[]		: register(t0, space10);
StructuredBuffer<float3> VertexStreams_Normal[]			: register(t0, space11);
StructuredBuffer<float4> VertexStreams_Tangent[]		: register(t0, space12);
StructuredBuffer<float2> VertexStreams_TexCoord[]		: register(t0, space13);
StructuredBuffer<float4> VertexStreams_Color[]			: register(t0, space14);

StructuredBuffer<uint> VertexStreams_Index[]			: register(t0, space15);

StructuredBuffer<VertexStreamInstanceIndices> VertexStreamIndices; // LUT