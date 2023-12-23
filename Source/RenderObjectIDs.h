#pragma once


namespace gr
{
	typedef uint32_t RenderDataID;
	constexpr RenderDataID k_invalidRenderObjectID = std::numeric_limits<uint32_t>::max();

	typedef uint32_t TransformID;
	constexpr TransformID k_invalidTransformID = std::numeric_limits<uint32_t>::max();

	// Default for special cases that don't need a Transform
	constexpr TransformID k_sharedIdentityTransformID = 0; 

	typedef uint32_t LightID;
	constexpr LightID k_invalidLightID = std::numeric_limits<uint32_t>::max();
}