// © 2025 Adam Badke. All rights reserved.
#version 460 // Suppress IDE warnings; Stripped out at compile time

#include "GBufferCommon.glsli"
#include "UVUtils.glsli"

#include "../Common/MaterialParams.h"
#include "../Common/TargetParams.h"


layout(binding = 0) uniform TargetParams { TargetData _TargetParams; };

layout(location = 0, rgba32f) coherent uniform image2D LightingTarget;


layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void CShader()
{
	const vec2 targetResolution = _TargetParams.g_targetDims.xy;
	ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);

	if (texelCoord.x >= targetResolution.x || texelCoord.y >= targetResolution.y)
	{
		return;
	}

	// OpenGL: Must flip the Y axis to counteract the flip when sampling the GBuffer
	const GBuffer gbuffer = UnpackGBuffer(vec2(texelCoord.x, targetResolution.y - gl_GlobalInvocationID.y));
	if (gbuffer.MaterialID != MAT_ID_GLTF_Unlit)
	{
		return;
	}

	imageStore(LightingTarget, texelCoord, vec4(gbuffer.LinearAlbedo.rgb, 1.f));
}