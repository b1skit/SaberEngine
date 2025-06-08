// © 2023 Adam Badke. All rights reserved.
#include "Private/LightRenderData.h"


namespace gr
{
	Light::RenderDataAmbientIBL::RenderDataAmbientIBL(
		char const* name, gr::RenderDataID renderDataID, gr::TransformID transformID)
		: m_renderDataID(renderDataID)
		, m_transformID(transformID)
	{
		strncpy(m_lightName, name, core::INamedObject::k_maxNameLength);
	}


	Light::RenderDataDirectional::RenderDataDirectional(
		char const* name, gr::RenderDataID renderDataID, gr::TransformID transformID)
		: m_renderDataID(renderDataID)
		, m_transformID(transformID)
	{
		strncpy(m_lightName, name, core::INamedObject::k_maxNameLength);
	}


	Light::RenderDataPoint::RenderDataPoint(
		char const* name, gr::RenderDataID renderDataID, gr::TransformID transformID)
		: m_renderDataID(renderDataID)
		, m_transformID(transformID)
	{
		strncpy(m_lightName, name, core::INamedObject::k_maxNameLength);
	}


	Light::RenderDataSpot::RenderDataSpot(
		char const* name, gr::RenderDataID renderDataID, gr::TransformID transformID)
		: m_renderDataID(renderDataID)
		, m_transformID(transformID)
	{
		strncpy(m_lightName, name, core::INamedObject::k_maxNameLength);
	}
}