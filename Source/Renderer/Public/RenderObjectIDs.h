#pragma once


namespace gr
{
	typedef uint32_t IDType;

	typedef IDType RenderDataID;
	constexpr RenderDataID k_invalidRenderDataID = std::numeric_limits<uint32_t>::max();

	typedef IDType TransformID;
	constexpr TransformID k_invalidTransformID = std::numeric_limits<uint32_t>::max(); // Identity Transform

	typedef uint32_t FeatureBitmask;

	enum RenderObjectFeature : FeatureBitmask
	{
		None					= 0,
		IsSceneBounds			= 1 << 0,
		IsMeshBounds			= 1 << 1,
		IsMeshPrimitiveBounds	= 1 << 2,
		IsLightBounds			= 1 << 3,
		
		IsMeshPrimitiveConcept	= 1 << 4,

		Invalid
	};

	// True if the featureBits contain the individual feature
	inline bool HasFeature(RenderObjectFeature feature, FeatureBitmask featureBits)
	{
		return feature == RenderObjectFeature::None || 
			featureBits & feature;
	}

	// True if the featureBits contain all of the features
	inline bool HasAllFeatures(FeatureBitmask features, FeatureBitmask featureBits)
	{
		return features == RenderObjectFeature::None ||
			(features & featureBits) == features;
	}
}