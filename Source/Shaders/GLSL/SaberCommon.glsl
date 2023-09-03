#ifndef SABER_COMMON
#define SABER_COMMON

// Saber Engine Shader Common
// Defines variables and structures common to all shaders

#define ALPHA_CUTOFF 0.1f

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

	// TODO: Support joints/weights
#endif


// Fragment shader specific properties:
//-------------------------------------

#if defined(SABER_FRAGMENT_SHADER)
	#if defined(SABER_VEC2_OUTPUT)
		layout (location = 0) out vec2 FragColor;
	#elif defined(SABER_VEC3_OUTPUT)
		layout (location = 0) out vec3 FragColor;
	#elif defined(SABER_VEC4_OUTPUT)
		layout (location = 0) out vec4 FragColor;
	#endif
	
	// Make our fragment coordinates ([0,xRes], [0,yRes]) match our uv (0,0) = top-left convention
	layout(origin_upper_left) in vec4 gl_FragCoord;
#endif



// Common shader properties:
//--------------------------

#if defined(SABER_VERTEX_SHADER)
	layout(location = 9) out struct VtoF	// Vertex output
#elif defined(SABER_FRAGMENT_SHADER)
	layout(location = 9) in struct VtoF		// Fragment input
#else
	struct VtoF	// Default/geometry in/out. If geometry, in & out must be bound to the same location
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
#else
	};
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
layout(binding=4) uniform sampler2D GBufferMatProp0;
layout(binding=5) uniform sampler2D GBufferDepth;

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

	// KHR_materials_emissive_strength: Multiplies emissive factor
	vec4 g_emissiveFactorStrength; // .xyz = emissive factor, .w = emissive strength

	// Non-GLTF properties:
	vec4 g_f0; // .xyz = f0, .w = unused. For non-metals only
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

	vec4 g_projectionParams; // .x = 1 (unused), .y = near, .z = far, .w = 1/far

	vec4 g_exposureProperties; // .x = exposure, .y = ev100, .z = bloom exposure compensation

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
	vec4 g_renderTargetResolution;
};


// GraphicsSystem_DeferredLighting.cpp
layout(std430, binding=3) readonly buffer AmbientLightParams
{
	uint g_maxPMREMMip;
};


// TODO: Wrap other buffer contents in a struct
struct InstancedMeshParamsCB
{
	mat4 g_model;
	mat4 g_transposeInvModel;
};
layout(std430, binding=4) readonly buffer InstancedMeshParams
{
	InstancedMeshParamsCB g_instancedMeshParams[]; // Variable-sized array: Must be the bottom-most variable in the block
};


// GraphicsSystem_Shadows.h
layout(std430, binding=5) readonly buffer CubemapShadowRenderParams
{
	mat4 g_cubemapShadowCam_VP[6];

	vec2 g_cubemapShadowCamNearFar; // .xy = near, far

	vec3 g_cubemapLightWorldPos;
};


// GraphicsSystem_DeferredLighting.cpp
layout(std430, binding=6) readonly buffer IEMPMREMGenerationParams
{
	vec4 g_numSamplesRoughness; // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness
};


// GraphicsSystem_Bloom.cpp
layout(std430, binding=7) readonly buffer BloomTargetParams
{
	vec4 g_bloomTargetResolution; // .x = xRes, .y = yRes, .z = 1/xRes, .w = 1/yRes
};

layout(std430, binding=8) readonly buffer BloomParams
{
	vec4 g_sigmoidParams; // .x = Sigmoid ramp power, .y = Sigmoid speed, .zw = unused
};


// GraphicsSystem_Skybox.cpp
layout(std430, binding=9) readonly buffer SkyboxParams
{
	vec4 g_skyboxTargetResolution; // .x = xRes, .y = yRes, .z = 1/xRes, .w = 1/yRes
};
#endif