#ifndef SABER_COMMON
#define SABER_COMMON

// Saber Engine Shader Common
// Defines variables and structures common to all shaders

// Vertex shader specific properties:
//-----------------------------------

#if defined(SABER_VERTEX_SHADER)
	layout(location = 0) in vec3 in_position;

	#if !defined(SABER_DEPTH)
		layout(location = 1) in vec4 in_color;
		layout(location = 2) in vec3 in_normal;
		layout(location = 3) in vec4 in_tangent;
		layout(location = 4) in vec2 in_uv0;	
	#endif
#endif


// Fragment shader specific properties:
//-------------------------------------

#if defined(SABER_FRAGMENT_SHADER) && defined(SABER_VEC2_OUTPUT)
	out vec2 FragColor;
#elif defined(SABER_FRAGMENT_SHADER) && defined(SABER_VEC3_OUTPUT)
	out vec3 FragColor;
#elif defined(SABER_FRAGMENT_SHADER) && defined(SABER_VEC4_OUTPUT)
	out vec4 FragColor;
#endif


// Common shader properties:
//--------------------------

#if defined(SABER_VERTEX_SHADER)
	layout(location = 9) out struct VtoF	// Vertex output
#elif defined(SABER_GEOMETRY_SHADER)
	struct VtoF								// Geometry in/out: Must be bound to the same location as both an in and out in the geometry shader
#elif defined(SABER_FRAGMENT_SHADER)
	layout(location = 9) in struct VtoF		// Fragment input
#endif
	{
		vec4 vertexColor;
		vec3 vertexWorldNormal;
		vec2 uv0;
		vec3 localPos;	// Received in_position: Local-space position
		vec3 worldPos;	// World-space position
		vec3 shadowPos;	// Shadowmap projection-space position

		mat3 TBN;		// Normal map change-of-basis matrix
#if defined(SABER_VERTEX_SHADER) || defined(SABER_FRAGMENT_SHADER)
	} data; // TODO: Rename this as vOut
#elif defined(SABER_GEOMETRY_SHADER)
	};
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
layout(binding=10) uniform sampler2D	Depth0;

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


// Mesh params:
uniform mat4 in_model; // Local -> World

// Shadow params:
uniform mat4 shadowCam_vp; // Shadow map: [Projection * View]
uniform float maxShadowBias; // Offsets for preventing shadow acne
uniform float minShadowBias;
uniform float shadowCam_near; // Near/Far planes of current shadow camera
uniform float shadowCam_far;

uniform vec4 texelSize;		// Depth map/GBuffer texel size: .xyzw = (1/width, 1/height, width, height)

// Target params:
uniform vec4 screenParams; // .x = xRes, .y = yRes, .z = 1/xRes, .w = 1/yRes


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


// Deferred point lights:
uniform vec3 lightColor;
uniform vec3 lightWorldPos;	// Light position in world space

// TODO: Point lights are currently drawn as individual meshes in a single stage. Need to implement a batch system, so
// we can draw them as an instanced batch with an array of indexed transforms & param values

layout(std430, binding=2) readonly buffer LightParams
{
	vec3 g_colorIntensity;

	vec3 g_worldPos; // Directional lights: Normalized, world-space dir pointing towards source (ie. parallel)
};

#endif