// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class EntityManager;
	class TransformComponent;


	class Mesh final
	{
	public:
		struct MeshConceptMarker final {};

	public:
		static void AttachMeshConceptMarker(entt::entity, char const* name);


		static void ShowImGuiWindow(fr::EntityManager&, entt::entity meshConcept);
		static void ShowImGuiSpawnWindow();
	};
}