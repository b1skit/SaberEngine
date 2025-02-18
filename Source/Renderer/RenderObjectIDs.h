#pragma once


namespace gr
{
	typedef uint32_t RenderDataID;
	constexpr RenderDataID k_invalidRenderDataID = std::numeric_limits<uint32_t>::max();

	typedef uint32_t TransformID;
	constexpr TransformID k_invalidTransformID = std::numeric_limits<uint32_t>::max(); // Identity Transform

	typedef uint32_t FeatureBitmask;

	enum RenderObjectFeature : uint32_t
	{
		IsSceneBounds			= 1 << 0,
		IsMeshBounds			= 1 << 1,
		IsMeshPrimitiveBounds	= 1 << 2,
		IsLightBounds			= 1 << 3,
		
		IsMeshPrimitiveConcept	= 1 << 4,

		Invalid
	};

	inline bool HasFeature(RenderObjectFeature feature, FeatureBitmask featureBits)
	{
		return featureBits & (1 << feature);
	}
}