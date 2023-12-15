// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Transform.h"
#include "RenderDataIDs.h"


// Automatically assigns itself a unique TransformID
class TransformComponent
{
	static constexpr auto in_place_delete = true; // Required for pointer stability

	TransformComponent(gr::Transform* parent) 
		: m_transformID(s_transformIDs.fetch_add(1))
		, m_transform(parent)
	{
	}


	gr::Transform& GetTransform()
	{
		return m_transform;
	}


	gr::TransformID GetTransformID() const
	{
		return m_transformID;
	}

private:
	gr::Transform m_transform;

	const gr::TransformID m_transformID;

private:
	static std::atomic<gr::RenderObjectID> s_transformIDs;
};


struct TransformRenderData
{
	glm::mat4 g_model;
	glm::mat4 g_transposeInvModel;

	gr::TransformID m_transformID;
};