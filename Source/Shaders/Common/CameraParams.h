// © 2024 Adam Badke. All rights reserved.
#ifndef SE_CAMERA_PARAMS
#define SE_CAMERA_PARAMS

#include "PlatformConversions.h"


struct CameraParamsData
{
	float4x4 g_view;
	float4x4 g_invView;
	float4x4 g_projection;
	float4x4 g_invProjection;
	float4x4 g_viewProjection;
	float4x4 g_invViewProjection;

	float4 g_projectionParams; // .x = near, .y = far, .z = 1/near, .w = 1/far

	float4 g_exposureProperties; // .x = exposure, .y = ev100, .zw = unused 
	float4 g_bloomSettings; // .x = strength, .yz = XY radius, .w = bloom exposure compensation

	float4 g_cameraWPos;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "CameraParams";
#endif
};


#endif // SE_CAMERA_PARAMS