#pragma once


namespace gr
{
	typedef uint32_t RenderObjectID;
	typedef uint32_t TransformID;

	constexpr RenderObjectID k_invalidRenderObjectID = std::numeric_limits<uint32_t>::max();
	constexpr TransformID k_invalidTransformID = std::numeric_limits<uint32_t>::max();
}