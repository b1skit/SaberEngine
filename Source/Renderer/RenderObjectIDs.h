#pragma once
#include "Core/RenderObjectIDs.h"


namespace gr
{
	// Forward declare the core types in the gr namespace for backward compatibility
	using IDType = core::IDType;
	using RenderDataID = core::RenderDataID;
	using TransformID = core::TransformID;
	using FeatureBitmask = core::FeatureBitmask;
	using RenderObjectFeature = core::RenderObjectFeature;
	
	constexpr RenderDataID k_invalidRenderDataID = core::k_invalidRenderDataID;
	constexpr TransformID k_invalidTransformID = core::k_invalidTransformID;

	// Forward declare the utility functions
	using core::HasFeature;
	using core::HasAllFeatures;
}