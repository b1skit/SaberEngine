// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/MeshPrimitive.h"


namespace core
{
	template<typename T>
	class InvPtr;
}

namespace pr
{
	class EntityManager;
	class RenderDataComponent;

	class MeshPrimitiveComponent
	{
	public:
		static entt::entity CreateMeshPrimitiveConcept(
			pr::EntityManager&,
			entt::entity owningEntity,
			core::InvPtr<gr::MeshPrimitive> const&,
			glm::vec3 const& positionMinXYZ,
			glm::vec3 const& positionMaxXYZ);

		static void AttachMeshPrimitiveComponent(
			pr::EntityManager&,
			entt::entity owningEntity,
			core::InvPtr<gr::MeshPrimitive> const&,
			glm::vec3 const& positionMinXYZ,
			glm::vec3 const& positionMaxXYZ);

		// Attach a MeshPrimitive without any of the typical dependencies (Bounds, Transforms, Material etc). This is
		// for special cases, such as deferred lights that require a fullscreen quad
		static MeshPrimitiveComponent& AttachRawMeshPrimitiveConcept(
			EntityManager&, entt::entity owningEntity, pr::RenderDataComponent const&, core::InvPtr<gr::MeshPrimitive> const&);


	public:
		static gr::MeshPrimitive::RenderData CreateRenderData(entt::entity, MeshPrimitiveComponent const&);

		static void ShowImGuiWindow(pr::EntityManager&, entt::entity lightEntity);


	public:
		core::InvPtr<gr::MeshPrimitive> m_meshPrimitive;
	};
}