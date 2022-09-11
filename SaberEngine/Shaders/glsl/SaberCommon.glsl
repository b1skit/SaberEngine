#ifndef SABER_COMMON
#define SABER_COMMON

// Saber Engine Shader Common
// Defines variables and structures common to all shaders

// Vertex shader specific properties:
//-----------------------------------

#if defined(SABER_VERTEX_SHADER)
	layout(location = 0) in vec3 in_position;
	layout(location = 1) in vec4 in_color;

	layout(location = 2) in vec3 in_normal;
	layout(location = 3) in vec3 in_tangent;
	layout(location = 4) in vec3 in_bitangent;

	layout(location = 5) in vec4 in_uv0;
	layout(location = 6) in vec4 in_uv1;
	layout(location = 7) in vec4 in_uv2;
	layout(location = 8) in vec4 in_uv3;
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

		vec4 uv0;
		vec4 uv1;
		vec4 uv2;
		vec4 uv3;

		vec3 localPos;			// Received in_position: Local-space position
		vec3 viewPos;			// Camera/eye-space position
		vec3 worldPos;			// World-space position
		vec3 shadowPos;			// Shadowmap projection-space position

		mat3 TBN;				// Normal map change-of-basis matrix
#if defined(SABER_VERTEX_SHADER) || defined(SABER_FRAGMENT_SHADER)
	} data; // TODO: Rename this as vOut
#elif defined(SABER_GEOMETRY_SHADER)
	};
#endif


// Forward Lighting:
uniform vec3 ambientColor;		// Deprecated: Use deferred lightColor instead

// Deferred key light:
uniform vec3 keylightWorldDir;	// Normalized, world-space, points towards keylight (ie. parallel)
uniform vec3 keylightViewDir;	// Normalized, view-space, points towards keylight (ie. parallel). Note: Currently only uploaded for deferred lights

// Deferred lights:
uniform vec3 lightColor;
uniform vec3 lightWorldPos;		// Light position in world space


// Matrices:
uniform mat4 in_model;				// Local -> World
uniform mat4 in_modelRotation;		// Local -> World, rotations ONLY (i.e. For transforming normals) TODO: Make this a mat3
uniform mat4 in_view;				// World -> View
uniform mat4 in_projection;			// View -> Projection
uniform mat4 in_mv;					// [View * Model]
uniform mat4 in_mvp;				// [Projection * View * Model]
uniform mat4 in_inverse_vp;			// [Projection * View]^-1


// Texture samplers:
// Note: The layout bindings must correspond with the Material's TextureSlotDesc index
													// TEXTURE:								FBX MATERIAL SOURCE SLOT:
// GBuffer stage input textures:					//---------								-------------------------
layout(binding=0) uniform sampler2D MatAlbedo;		// Albedo (RGB) + transparency (A)		Diffuse/color
layout(binding=1) uniform sampler2D MatNormal;		// Tangent-space normals (RGB)			Bump
layout(binding=2) uniform sampler2D MatRMAO;		// Roughness, Metalic, MatAlbedo		Specular
layout(binding=3) uniform sampler2D MatEmissive;	// Emissive (RGB)						Incandescence

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

// Generic material properties:
uniform vec4		MatProperty0;	// .rgb = F0 (Surface response at 0 degrees), .a = Phong exponent


// TODO: Pack common parameters into structured buffers
uniform vec4		texelSize;		// Depth map/GBuffer texel size: .xyzw = (1/width, 1/height, width, height)

// Shadow map parameters:
uniform mat4		shadowCam_vp;	// Shadow map: [Projection * View]

uniform float		maxShadowBias;	// Offsets for preventing shadow acne
uniform float		minShadowBias;

uniform float		shadowCam_near;	// Near/Far planes of current shadow camera
uniform float		shadowCam_far;

// System variables:
uniform vec4 screenParams;			// .x = xRes, .y = yRes, .z = 1/xRes, .w = 1/yRes

uniform vec3 cameraWPos;	// World-space camera position


#endif