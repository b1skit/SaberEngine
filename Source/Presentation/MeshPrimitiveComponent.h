// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "BoundsComponent.h"
#include "RenderDataComponent.h"

#include "Renderer/MeshPrimitive.h"


namespace fr
{
	class EntityManager;


	class MeshPrimitiveComponent
	{
	public:
		static entt::entity CreateMeshPrimitiveConcept(
			fr::EntityManager&,
			entt::entity owningEntity,
			gr::MeshPrimitive const*,
			glm::vec3 const& positionMinXYZ = fr::BoundsComponent::k_invalidMinXYZ, // Default: Compute bounds manually
			glm::vec3 const& positionMaxXYZ = fr::BoundsComponent::k_invalidMaxXYZ); // Default: Compute bounds manually

		static void AttachMeshPrimitiveComponent(
			fr::EntityManager&,
			entt::entity owningEntity,
			gr::MeshPrimitive const*,
			glm::vec3 const& positionMinXYZ = fr::BoundsComponent::k_invalidMinXYZ, // Default: Compute bounds manually
			glm::vec3 const& positionMaxXYZ = fr::BoundsComponent::k_invalidMaxXYZ); // Default: Compute bounds manually

		// Attach a MeshPrimitive without any of the typical dependencies (Bounds, Transforms, Material etc). This is
		// for special cases, such as deferred lights that require a fullscreen quad
		static MeshPrimitiveComponent& AttachRawMeshPrimitiveConcept(
			EntityManager&, entt::entity owningEntity, gr::RenderDataComponent const&, gr::MeshPrimitive const*);


	public:
		static gr::MeshPrimitive::RenderData CreateRenderData(entt::entity, MeshPrimitiveComponent const&);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity lightEntity);


	public:
		gr::MeshPrimitive const* m_meshPrimitive;
	};
}