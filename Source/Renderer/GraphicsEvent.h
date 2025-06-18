// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/Util/CHashKey.h"


namespace greventkey
{
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
			void const*>;

		GraphicsEventData m_data;
	};
}