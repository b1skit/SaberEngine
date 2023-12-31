// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Transform.h"
#include "TransformRenderData.h"
#include "RenderObjectIDs.h"


namespace fr
{
	class EntityManager;


	// EnTT wrapper for a fr::Transform, to guarantee pointer stability.
	// Automatically assigns itself a unique TransformID
	class TransformComponent
	{
	public:
		static constexpr auto in_place_delete = true; // Required for pointer stability


	public:
		struct NewIDMarker {}; // Attached when a a new TransformID is allocated

		static TransformComponent& AttachTransformComponent(fr::EntityManager&, entt::entity, fr::Transform* parent);

		static gr::Transform::RenderData CreateRenderData(fr::TransformComponent&);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity owningEntity);


	public:
		fr::Transform& GetTransform();
		fr::Transform const& GetTransform() const;

		gr::TransformID GetTransformID() const;


	public: // Transform systems:
		static void DispatchTransformUpdateThreads(
			std::vector<std::future<void>>& taskFuturesOut, fr::Transform* rootNode);


	private:
		fr::Transform m_transform;

		const gr::TransformID m_transformID;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		TransformComponent(PrivateCTORTag, fr::Transform* parent);


	private: // Static TransformID functionality:
		static std::atomic<gr::TransformID> s_transformIDs;
	};


	inline fr::Transform& TransformComponent::GetTransform()
	{
		return m_transform;
	}


	inline fr::Transform const& TransformComponent::GetTransform() const
	{
		return m_transform;
	}


	inline gr::TransformID TransformComponent::GetTransformID() const
	{
		return m_transformID;
	}


	// ---


	class UpdateTransformDataRenderCommand
	{
	public:
		UpdateTransformDataRenderCommand(fr::TransformComponent&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::TransformID m_transformID;
		const gr::Transform::RenderData m_data;
	};	
}