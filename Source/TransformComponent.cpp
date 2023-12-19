// © 2023 Adam Badke. All rights reserved.
#include "TransformComponent.h"


namespace fr
{
	std::atomic<gr::TransformID> TransformComponent::s_transformIDs = 0;


	TransformComponent::TransformComponent(gr::Transform* parent)
		: m_transformID(s_transformIDs.fetch_add(1))
		, m_transform(parent)
	{
	}
}