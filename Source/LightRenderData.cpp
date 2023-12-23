// © 2023 Adam Badke. All rights reserved.
#include "Assert.h"
#include "LightRenderData.h"


namespace gr
{
	Light::RenderData::RenderData(
		gr::Light::LightType lightType, gr::LightID lightID, gr::RenderDataID renderDataID, gr::TransformID transformID)
		: m_lightType(lightType)
		, m_lightID(lightID)
		, m_renderDataID(renderDataID)
		, m_transformID(transformID)
	{
		memset(&m_typeProperties, 0, sizeof(m_typeProperties));

		m_diffuseEnabled = true;
		m_specularEnabled = true;
	}


	Light::RenderData::~RenderData()
	{
		// Need a DTOR as we have a union, but no specific cleanup needed as we're just storing data and raw ptrs
	};


	Light::RenderData& Light::RenderData::operator=(Light::RenderData const& rhs)
	{
		if (&rhs == this)
		{
			return *this;
		}

		memcpy(this, &rhs, sizeof(Light::RenderData)); // RenderData is trivially copyable

		return *this;
	}
}