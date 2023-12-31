// © 2023 Adam Badke. All rights reserved.
#include "Assert.h"
#include "LightRenderData.h"


namespace gr
{
	Light::RenderData::RenderData(
		char const* name,
		gr::Light::LightType lightType, 
		gr::LightID lightID, 
		gr::RenderDataID renderDataID, 
		gr::TransformID transformID)
		: m_lightType(lightType)
		, m_lightID(lightID)
		, m_renderDataID(renderDataID)
		, m_transformID(transformID)
	{
		// CTOR just sets the identifiers and zeros everything else out. 
		// Everything else is populated in fr::LightComponent::CreateRenderData

		memset(&m_typeProperties, 0, sizeof(m_typeProperties));
		strncpy(m_lightName, name, en::NamedObject::k_maxNameLength);
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