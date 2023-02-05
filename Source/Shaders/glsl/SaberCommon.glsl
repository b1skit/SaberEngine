#ifndef SABER_COMMON
#define SABER_COMMON

// Saber Engine Shader Common
// Defines variables and structures common to all shaders

// Vertex shader specific properties:
//-----------------------------------

#if defined(SABER_VERTEX_SHADER)
	layout(location = 0) in vec3 in_position;

	#if !defined(SABER_DEPTH)
		layout(location = 1) in vec3 in_normal;
		layout(location = 2) in vec4 in_tangent;
		layout(location = 3) in vec2 in_uv0;
		layout(location = 4) in vec4 in_color;
	#endif
#endif


// Fragment shader specific properties:
//-------------------------------------

#if defined(SABER_FRAGMENT_SHADER)
	#if defined(SABER_VEC2_OUTPUT)
		out vec2 FragColor;
	#elif defined(SABER_VEC3_OUTPUT)
		out vec3 FragColor;
	#elif defined(SABER_VEC4_OUTPUT)
		out vec4 FragColor;
	#endif
#endif


// Common shader properties:
//--------------------------

#if defined(SABER_VERTEX_SHADER)
	layout(location = 9) out struct VtoF	// Vertex output
#elif defined(SABER_FRAGMENT_SHADER)
	layout(location = 9) in struct VtoF		// Fragment input
#else
	struct VtoF								// Default/geometry in/out. If geometry, in & out must be bound to the same location
#endif
	{
		vec4 vertexColor;
		vec3 vertexWorldNormal;
		vec2 uv0;
		vec3 localPos;	// Received in_position: Local-space position
		vec3 worldPos;	// World-space position
		vec3 shadowPos;	// Shadowmap projection-space position

		mat3 TBN;		// Normal map change-of-basis matrix

#if defined(SABER_VERTEX_SHADER) || defined(SABER_FRAGMENT_SHADER) || defined(SABER_GEOMETRY_SHADER)
	} vOut;
#endif

// Dynamic uniforms for instancing:
#if defined(SABER_INSTANCING)
	#if defined(SABER_VERTEX_SHADER)
		flat out uint instanceID;
	#elif defined(SABER_FRAGMENT_SHADER)
		flat in uint instanceID;
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
layout(binding=4) uniform sampler2D GBufferWPos;
layout(binding=5) uniform sampler2D GBufferMatProp0;
layout(binding=6) uniform sampler2D GBufferDepth;

// Deferred light shadowmaps:
layout(binding=10) uniform sampler2D Depth0;

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


// Note: Must match the PBRMetallicRoughnessParams struct defined in Material.h, without any padding
layout(std430, binding=0) readonly buffer PBRMetallicRoughnessParams
{
	vec4 g_baseColorFactor;
	float g_metallicFactor;
	float g_roughnessFactor;
	float g_normalScale;
	float g_occlusionStrength;
	float g_emissiveStrength;
	vec3 g_emissiveFactor;
	vec3 g_f0; // For non-metals only
};


// Camera.h::CameraParams
layout(std430, binding=1) readonly buffer CameraParams
{
	mat4 g_view;				// World -> View
	mat4 g_invView;				// View -> World
	mat4 g_projection;			// View -> Projection
	mat4 g_invProjection;		// Projection -> view
	mat4 g_viewProjection;		// Projection x View: World -> Projection
	mat4 g_invViewProjection;	// [Projection * View]^-1

	vec4 g_projectionParams;	// .x = 1 (unused), .y = near, .z = far, .w = 1/far
	
	vec3 g_cameraWPos;
};


// GraphicsSystem_DeferredLighting.cpp
layout(std430, binding=2) readonly buffer LightParams
{
	vec3 g_lightColorIntensity;

	// Directional lights: Normalized, world-space dir pointing towards source (ie. parallel)
	vec3 g_lightWorldPos;

	vec4 g_shadowMapTexelSize;	// .xyzw = width, height, 1/width, 1/height
	vec2 g_shadowCamNearFar;
	vec2 g_shadowBiasMinMax; // .xy = min, max shadow bias
	mat4 g_shadowCam_VP;
};


// GraphicsSystem_DeferredLighting.cpp
layout(std430, binding=3) readonly buffer AmbientLightParams
{
	uint g_maxPMREMMip;
};


layout(std430, binding=4) readonly buffer InstancedMeshParams
{
	mat4 g_model[]; // Variable-sized array: Must be the bottom-most variable in the block
};


// GraphicsSystem_Shadows.h
layout(std430, binding=5) readonly buffer CubemapShadowRenderParams
{
	mat4 g_cubemapShadowCam_VP[6];

	vec2 g_cubemapShadowCamNearFar; // .xy = near, far

	vec3 g_cubemapLightWorldPos;
};


// TextureTarget.cpp
layout(std430, binding=6) readonly buffer RenderTargetParams
{
	vec4 g_targetResolution; // .x = xRes, .y = yRes, .z = 1/xRes, .w = 1/yRes
};


// GraphicsSystem_Tonemapping.cpp
layout(std430, binding=7) readonly buffer TonemappingParams
{
	vec4 g_exposure; // .x = exposure, .yzw = unused
};


// GraphicsSystem_DeferredLighting.cpp
layout(std430, binding=8) readonly buffer IEMPMREMGenerationParams
{
	vec4 g_numSamplesRoughness; // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness
};
#endif