#pragma once


namespace gr
{
	typedef uint32_t RenderDataID;
	constexpr RenderDataID k_invalidRenderDataID = std::numeric_limits<uint32_t>::max();

	typedef uint32_t TransformID;
	constexpr TransformID k_invalidTransformID = std::numeric_limits<uint32_t>::max();

	typedef uint32_t FeatureBitmask;

	enum RenderObjectFeature : uint32_t
	{
		IsSceneBounds			= 0x0,
		IsMeshBounds			= 0x1,
		IsMeshPrimitiveBounds	= 0x2,
		
		IsMeshPrimitive			= 0x4,

		Invalid
	};

	inline bool HasFeature(RenderObjectFeature feature, FeatureBitmask featureBits)
	{
		return featureBits & (1 << feature);
	}
}