// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "MeshPrimitive.h"


namespace fr
{
	class MeshPrimitive
	{
	public:
		struct MeshPrimitiveComponent
		{
			// MeshPrimitives are held in the SceneData so duplicate data can be shared
			gr::MeshPrimitive const* m_meshPrimitive;
		};

	public:
		static gr::MeshPrimitive::RenderData CreateRenderData(MeshPrimitiveComponent const&);


	public:
		static entt::entity AttachMeshPrimitiveConcept(
			entt::entity meshConcept,
			char const* name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass fr::BoundsConcept::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass fr::BoundsConcept::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);
	};
}