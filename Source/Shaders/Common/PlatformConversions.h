// © 2024 Adam Badke. All rights reserved.
#ifndef SE_PLATFORM_CONVERSIONS
#define SE_PLATFORM_CONVERSIONS


// Convert HLSL types:
#if defined(__cplusplus)

#define uint uint32_t
#define uint2 glm::uvec2
#define uint3 glm::uvec3
#define uint4 glm::uvec4

#define int2 glm::ivec2
#define int3 glm::ivec3
#define int4 glm::ivec4

#define float2 glm::vec2
#define float3 glm::vec3
#define float4 glm::vec4

#define float2x2 glm::mat2
#define float3x3 glm::mat3
#define float4x4 glm::mat4

#elif defined(SE_OPENGL)

#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4

#define float2 vec2
#define float3 vec3
#define float4 vec4

#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4

#endif

// Hijack HLSL's built-in preprocessor macros to detect if we're in shader code
#if defined(__HLSL_VERSION)

// Payload access qualifiers (PAQ's):
#define raypayload [raypayload]
#define read(...) : read(__VA_ARGS__)
#define write(...) : write(__VA_ARGS__)

#elif defined(__cplusplus)

#define raypayload
#define read(...)
#define write(...)

#endif

#endif // SE_PLATFORM_CONVERSIONS