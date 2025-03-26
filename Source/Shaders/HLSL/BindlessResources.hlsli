// © 2025 Adam Badke. All rights reserved.
#include "../Common/BindlessResourceParams.h"

// Bindless resources: We use register spaces to overlap unbounded arrays on register 0

// Bindless LUT: Maps resources to their indexes in resource arrays
ConstantBuffer<BindlessLUTData> BindlessLUT[] : register(b0, space10);


// Vertex streams:
StructuredBuffer<float3> VertexStreams_Position[]		: register(t0, space11);
StructuredBuffer<float3> VertexStreams_Normal[]			: register(t0, space12);
StructuredBuffer<float4> VertexStreams_Tangent[]		: register(t0, space13);
StructuredBuffer<float2> VertexStreams_TexCoord[]		: register(t0, space14);
StructuredBuffer<float4> VertexStreams_Color[]			: register(t0, space15);

StructuredBuffer<uint16_t> VertexStreams_Index[]		: register(t0, space16);
