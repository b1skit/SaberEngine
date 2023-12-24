// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class Mesh
	{
	public:
		struct MeshConceptMarker {};

	public:
		static entt::entity AttachMeshConcept(entt::entity, char const* name);


		void ShowImGuiWindow();
	};
}