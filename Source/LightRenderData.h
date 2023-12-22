// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"


namespace gr
{
	class Light
	{
	public:
		enum LightType : uint8_t
		{
			AmbientIBL_Deferred,
			Directional_Deferred,
			Point_Deferred,

			LightType_Count
		};


		struct RenderData
		{
			gr::LightID m_lightID;
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;
		};
	};
}