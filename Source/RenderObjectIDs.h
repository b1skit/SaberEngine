#pragma once


namespace gr
{
	typedef uint32_t RenderDataID;
	constexpr RenderDataID k_invalidRenderObjectID = std::numeric_limits<uint32_t>::max();

	typedef uint32_t TransformID;
	constexpr TransformID k_invalidTransformID = std::numeric_limits<uint32_t>::max();

	// Default for special cases that don't need a Transform
	constexpr TransformID k_sharedIdentityTransformID = 0;

	typedef uint32_t FeatureBitmask;

	enum RenderObjectFeature
	{
		IsSceneBounds = 0x0,
		IsMeshBounds = 0x1, // If these are both not set, we can assume a Bounds is attached to a MeshPrimitive
		//... = 0x2
		//... = 0x4

		Invalid
	};

	inline bool HasFeature(RenderObjectFeature feature, FeatureBitmask featureBits)
	{
		return featureBits & (1 << feature);
	}
}