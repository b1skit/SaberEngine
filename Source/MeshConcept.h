// � 2022 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class EntityManager;
	class TransformComponent;


	class Mesh
	{
	public:
		struct MeshConceptMarker {};

	public:
		static void AttachMeshConcept(entt::entity, char const* name);


		static void ShowImGuiWindow(fr::EntityManager&, entt::entity meshConcept);
		static void ShowImGuiSpawnWindow();
	};
}