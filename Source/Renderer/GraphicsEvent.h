// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"

#include "Core/Util/CHashKey.h"


namespace greventkey
{
	constexpr util::CHashKey k_activeAmbientLightHasChanged("ActiveAmbientLightHasChanged");
	constexpr util::CHashKey k_triggerTemporalAccumulationReset("TriggerTemporalAccumulationReset");

	constexpr util::CHashKey GS_Shadows_DirectionalShadowArrayUpdated("GS_Shadows_DirectionalShadowArrayUpdated");
	constexpr util::CHashKey GS_Shadows_PointShadowArrayUpdated("GS_Shadows_PointShadowArrayUpdated");
	constexpr util::CHashKey GS_Shadows_SpotShadowArrayUpdated("GS_Shadows_SpotShadowArrayUpdated");
}

namespace gr
{
	struct GraphicsEvent
	{
		util::CHashKey m_eventKey = util::CHashKey("UninitializedEvent");

		using GraphicsEventData = std::variant<
			bool,
			void const*,
			gr::RenderDataID>;

		GraphicsEventData m_data;
	};
}