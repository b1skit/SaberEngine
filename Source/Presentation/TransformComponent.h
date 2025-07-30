// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Transform.h"

#include "Renderer/TransformRenderData.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderObjectIDs.h"


namespace pr
{
	class EntityManager;


	// EnTT wrapper for a pr::Transform, to guarantee pointer stability.
	// Automatically assigns itself a unique TransformID
	class TransformComponent
	{
	public:
		static constexpr auto in_place_delete = true; // Required for pointer stability


	public:
		struct NewIDMarker {}; // Attached when a a new TransformID is allocated

		static TransformComponent& AttachTransformComponent(pr::EntityManager&, entt::entity);

		static gr::Transform::RenderData CreateRenderData(pr::EntityManager&, pr::TransformComponent&);

		static void ShowImGuiWindow(pr::EntityManager&, entt::entity owningEntity, uint64_t uniqueID);


	public:
		pr::Transform& GetTransform();
		pr::Transform const& GetTransform() const;

		gr::TransformID GetTransformID() const;


	public: // Transform systems:
		static void DispatchTransformUpdateThreads(
			std::vector<std::future<void>>& taskFuturesOut, pr::Transform* rootNode);


	private:
		pr::Transform m_transform;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		TransformComponent(PrivateCTORTag, pr::Transform* parent);
	};


	inline pr::Transform& TransformComponent::GetTransform()
	{
		return m_transform;
	}


	inline pr::Transform const& TransformComponent::GetTransform() const
	{
		return m_transform;
	}


	inline gr::TransformID TransformComponent::GetTransformID() const
	{
		return m_transform.GetTransformID();
	}


	// ---


	class UpdateTransformDataRenderCommand final : public virtual gr::RenderCommand
	{
	public:
		UpdateTransformDataRenderCommand(pr::EntityManager&, pr::TransformComponent&);

		static void Execute(void*);

	private:
		const gr::TransformID m_transformID;
		const gr::Transform::RenderData m_data;
	};	
}