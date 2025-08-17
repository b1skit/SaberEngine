// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace pr
{
	class EntityManager;
	class TransformComponent;


	class Mesh final
	{
	public:
		struct MeshConceptMarker final {};

	public:
		static void AttachMeshConceptMarker(pr::EntityManager&, entt::entity, char const* name);


		static void ShowImGuiWindow(pr::EntityManager&, entt::entity meshConcept);
		static void ShowImGuiSpawnWindow(pr::EntityManager& em);
	};
}