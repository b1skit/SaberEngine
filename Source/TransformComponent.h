// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Transform.h"
#include "RenderDataIDs.h"


namespace fr
{
	// EnTT wrapper for a gr::Transform, to guarantee pointer stability.
	// Automatically assigns itself a unique TransformID
	class TransformComponent
	{
	public:
		static constexpr auto in_place_delete = true; // Required for pointer stability


	public:
		struct RenderData
		{
			glm::mat4 g_model;
			glm::mat4 g_transposeInvModel;

			gr::TransformID m_transformID;
		};


	public:
		TransformComponent(gr::Transform* parent);

		gr::Transform& GetTransform();
		gr::TransformID GetTransformID() const;


	private:
		gr::Transform m_transform;
		const gr::TransformID m_transformID;


	private:
		static std::atomic<gr::TransformID> s_transformIDs;
	};


	inline gr::Transform& TransformComponent::GetTransform()
	{
		return m_transform;
	}


	inline gr::TransformID TransformComponent::GetTransformID() const
	{
		return m_transformID;
	}
}