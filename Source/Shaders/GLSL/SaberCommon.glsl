#ifndef SABER_COMMON
#define SABER_COMMON

// Buffer definitions:
#include "../Common/BloomComputeParams.h"
#include "../Common/CameraParams.h"
#include "../Common/IBLGenerationParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/LightParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/ShadowRenderParams.h"
#include "../Common/SkyboxParams.h"
#include "../Common/TargetParams.h"


// Vertex shader inputs:
//----------------------

#if defined(SE_VERTEX_SHADER)
	layout(location = 0) in vec3 in_position;
	
#ifdef VIN_NORMAL
	layout(location = 1) in vec3 in_normal;
#endif
#ifdef VIN_TANGENT
	layout(location = 2) in vec4 in_tangent;
#endif
#ifdef VIN_UV0
	layout(location = 3) in vec2 in_uv0;
#endif
#ifdef VIN_COLOR
	layout(location = 4) in vec4 in_color;
#endif

	// TODO: Support joints/weights
#endif


// Common shader properties:
//--------------------------

struct VertexOut
{
#ifdef VOUT_WORLD_NORMAL
	vec3 WorldNormal;
#endif

#ifdef VOUT_LOCAL_POS
	vec3 LocalPos; // Received in_position: Local-space position
#endif

#ifdef VOUT_WORLD_POS
	vec3 WorldPos; // World-space position
#endif

#ifdef VOUT_TBN
	mat3 TBN; // Normal map change-of-basis matrix
#endif

	// The GLSL compiler gets confused if we define an empty struct; Assume uv0 is always required
	vec2 uv0;

#ifdef VOUT_COLOR
	vec4 Color;
#endif
	
};


#if defined(SE_VERTEX_SHADER)
	layout(location = 6) out VertexOut Out;
#elif defined(SE_GEOMETRY_SHADER)
	layout(location = 6) in VertexOut In[];
	layout(location = 6) out VertexOut Out;
#elif defined(SE_FRAGMENT_SHADER)
	layout(location = 6) in VertexOut In;
#endif


// Fragment shader outputs:
//-------------------------

#if defined(SE_FRAGMENT_SHADER)
	#if defined(SABER_VEC2_OUTPUT)
		layout (location = 0) out vec2 FragColor;
	#elif defined(SABER_VEC3_OUTPUT)
		layout (location = 0) out vec3 FragColor;
	#elif defined(SABER_VEC4_OUTPUT)
		layout (location = 0) out vec4 FragColor;
	#endif
#endif


// Instancing:
#if defined(SABER_INSTANCING)
struct InstanceParams
{
	uint InstanceID;
};

	#if defined(SE_VERTEX_SHADER)
		layout(location = 5) flat out InstanceParams InstanceParamsOut;

	#elif defined(SE_GEOMETRY_SHADER)
		layout(location = 5) flat in InstanceParams InstanceParamsIn[];
		layout(location = 5) flat out InstanceParams InstanceParamsOut;

	#elif defined(SE_FRAGMENT_SHADER)		
		layout(location = 5) flat in InstanceParams InstanceParamsIn;

	#endif 
#endif // SABER_INSTANCING


// GLTF PBR material input textures:
// Note: The layout bindings must correspond with the Material's TextureSlotDesc index
layout(binding=0) uniform sampler2D MatAlbedo;
layout(binding=1) uniform sampler2D MatMetallicRoughness;
layout(binding=2) uniform sampler2D MatNormal;
layout(binding=3) uniform sampler2D MatOcclusion;
layout(binding=4) uniform sampler2D MatEmissive;


// Lighting stage GBuffer textures:
layout(binding=0) uniform sampler2D GBufferAlbedo;
layout(binding=1) uniform sampler2D GBufferWNormal;
layout(binding=2) uniform sampler2D GBufferRMAO;
layout(binding=3) uniform sampler2D GBufferEmissive;
layout(binding=4) uniform sampler2D GBufferMatProp0;
layout(binding=5) uniform sampler2D GBufferDepth;

// Deferred light shadowmaps:
layout(binding=10) uniform sampler2DShadow Depth0;

// Generic texture samplers:
layout(binding=0) uniform sampler2D Tex0;
layout(binding=1) uniform sampler2D Tex1;
layout(binding=2) uniform sampler2D Tex2;
layout(binding=3) uniform sampler2D Tex3;
layout(binding=4) uniform sampler2D Tex4;
layout(binding=5) uniform sampler2D Tex5;
layout(binding=6) uniform sampler2D Tex6;
layout(binding=7) uniform sampler2D Tex7;
layout(binding=8) uniform sampler2D Tex8;

// Cube map samplers:
layout(binding=11) uniform samplerCube CubeMap0;
layout(binding=12) uniform samplerCube CubeMap1;

layout(binding=13) uniform samplerCubeShadow CubeDepth;

// Buffer bindings:
// Note: The binding locations are required here because we don't set them on the CPU side
// Note: OpenGL gives very strange, nonsensical compile errors if the members of our uniform blocks are structs, and
// their names are capitalized (but only with some letters!). Totally puzzling - but the '_' prefix is preferred anyway

layout(std430, binding=0) uniform InstanceIndexParams {	InstanceIndexData _InstanceIndexParams; };

// UBOs can't have a dynamic length; We use SSBOs for instancing instead
layout(std430, binding=1) readonly buffer InstancedTransformParams { InstancedTransformData _InstancedTransformParams[]; };
layout(std430, binding=2) readonly buffer InstancedPBRMetallicRoughnessParams {	InstancedPBRMetallicRoughnessData _InstancedPBRMetallicRoughnessParams[]; };

layout(std430, binding=3) readonly buffer DirectionalLightParams { LightData _DirectionalLightParams[]; };
layout(std430, binding=4) readonly buffer PointLightParams { LightData _PointLightParams[]; };
layout(std430, binding=5) readonly buffer SpotLightParams { LightData _SpotLightParams[]; };

layout(std430, binding=6) uniform LightIndexParams { LightIndexData _LightIndexParams; };

layout(std430, binding=7) uniform CameraParams { CameraData _CameraParams; };
layout(std430, binding=8) uniform PoissonSampleParams { PoissonSampleParamsData _PoissonSampleParams; };
layout(std430, binding=9) uniform AmbientLightParams { AmbientLightData _AmbientLightParams; };
layout(std430, binding=10) uniform CubemapShadowRenderParams { CubemapShadowRenderData _CubemapShadowRenderParams; };
layout(std430, binding=11) uniform IEMPMREMGenerationParams { IEMPMREMGenerationData _IEMPMREMGenerationParams; };
layout(std430, binding=12) uniform BloomComputeParams { BloomComputeData _BloomComputeParams; };
layout(std430, binding=13) uniform SkyboxParams { SkyboxData _SkyboxParams; };
layout(std430, binding=14) uniform TargetParams { TargetData _TargetParams; };

#endif