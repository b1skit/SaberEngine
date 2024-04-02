#ifndef SABER_COMMON
#define SABER_COMMON

#define ALPHA_CUTOFF 0.1f

// Buffer definitions:
#include "../Common/BloomComputeParams.h"
#include "../Common/CameraParams.h"
#include "../Common/IBLGenerationParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/LightParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/ShadowRenderParams.h"
#include "../Common/SkyboxParams.h"


// Vertex shader specific properties:
//-----------------------------------

#if defined(SE_VERTEX_SHADER)
	layout(location = 0) in vec3 in_position;

	#if !defined(SABER_DEPTH)
		layout(location = 1) in vec3 in_normal;
		layout(location = 2) in vec4 in_tangent;
		layout(location = 3) in vec2 in_uv0;
		layout(location = 4) in vec4 in_color;
	#endif

	// TODO: Support joints/weights
#endif


// Fragment shader specific properties:
//-------------------------------------

#if defined(SE_FRAGMENT_SHADER)
	#if defined(SABER_VEC2_OUTPUT)
		layout (location = 0) out vec2 FragColor;
	#elif defined(SABER_VEC3_OUTPUT)
		layout (location = 0) out vec3 FragColor;
	#elif defined(SABER_VEC4_OUTPUT)
		layout (location = 0) out vec4 FragColor;
	#endif
#endif


// Common shader properties:
//--------------------------

#if defined(SE_VERTEX_SHADER)
	layout(location = 9) out struct VtoF	// Vertex output
#elif defined(SE_FRAGMENT_SHADER)
	layout(location = 9) in struct VtoF		// Fragment input
#else
	struct VtoF	// Default/geometry in/out. If geometry, in & out must be bound to the same location
#endif
	{
		vec4 Color;
		vec2 uv0;

#ifdef VOUT_WORLD_NORMAL
		vec3 WorldNormal;
#endif
#ifdef VOUT_LOCAL_POS
		vec3 LocalPos; // Received in_position: Local-space position
#endif
#ifdef VOUT_WORLD_POS
		vec3 worldPos; // World-space position
#endif
#ifdef VOUT_TBN
		mat3 TBN; // Normal map change-of-basis matrix
#endif

#if defined(SE_VERTEX_SHADER) || defined(SE_FRAGMENT_SHADER) || defined(SE_GEOMETRY_SHADER)
	} vOut;
#else
	};
#endif

// Dynamic uniforms for instancing:
#if defined(SABER_INSTANCING)
	#if defined(SE_VERTEX_SHADER)
		flat out uint InstanceID;
	#elif defined(SE_FRAGMENT_SHADER)
		flat in uint InstanceID;
	#endif
#endif


// Texture samplers:
// Note: The layout bindings must correspond with the Material's TextureSlotDesc index

// GLTF PBR material input textures:
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

// Note: OpenGL gives very strange, nonsensical compile errors if the members of our uniform blocks are structs, and
// their names are capitalized (but only with some letters!). Totally puzzling - but the '_' prefix is preferred anyway

layout(std430, binding=0) uniform InstanceIndexParams {	InstanceIndexData _InstanceIndexParams; };

// UBOs can't have a dynamic length; We use SSBOs for instancing instead
layout(std430, binding=1) readonly buffer InstancedTransformParams { InstancedTransformData _InstancedTransformParams[]; };
layout(std430, binding=2) readonly buffer InstancedPBRMetallicRoughnessParams {	InstancedPBRMetallicRoughnessData _InstancedPBRMetallicRoughnessParams[]; };

layout(std430, binding=3) uniform CameraParams { CameraData _CameraParams; };
layout(std430, binding=4) uniform LightParams { LightData _LightParams; };
layout(std430, binding=5) uniform PoissonSampleParams { PoissonSampleParamsData _PoissonSampleParams; };
layout(std430, binding=6) uniform AmbientLightParams { AmbientLightData _AmbientLightParams; };
layout(std430, binding=7) uniform CubemapShadowRenderParams { CubemapShadowRenderData _CubemapShadowRenderParams; };
layout(std430, binding=8) uniform IEMPMREMGenerationParams { IEMPMREMGenerationData _IEMPMREMGenerationParams; };
layout(std430, binding=9) uniform BloomComputeParams { BloomComputeData _BloomComputeParams; };
layout(std430, binding=10) uniform SkyboxParams { SkyboxData _SkyboxParams; };

#endif