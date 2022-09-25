#pragma once

#include <vector>
#include <memory>

#include "Mesh.h"

namespace gr
{	
	// TODO: This should be in the fr namespace -> Meshes are gr objects, RenderMeshes are fr objects
	

	// Contains a set of mesh primitives
	class RenderMesh
	{
	public:
		RenderMesh(gr::Transform* gameObjectParent, std::shared_ptr<gr::Mesh> meshPrimitive);

		RenderMesh(RenderMesh const&);
		RenderMesh(RenderMesh&&) = default;
		RenderMesh& operator=(RenderMesh const&) = default;
		~RenderMesh() = default;	

		RenderMesh() = delete;

		// Getters/Setters:
		inline gr::Transform& GetTransform() { return m_transform; } // BEWARE: DOESN'T UPDATE CHILD MESH TRANSFORMS
		// -> NOT A RISK ONCE WE REMOVE TRANSFORMS FROM MESH PRIMITIVES
		// -> CALLING AddChildMeshPrimitive SETS CHILD MESH PRIMITIVE TRANSFORMS
		inline gr::Transform const& GetTransform() const { return m_transform; }

		void AddChildMeshPrimitive(std::shared_ptr<gr::Mesh> mesh);
		inline std::vector<std::shared_ptr<gr::Mesh>> const& GetChildMeshPrimitives() const { return m_meshPrimitives; }


	private:
		std::vector<std::shared_ptr<gr::Mesh>> m_meshPrimitives;  // Pointers to Mesh objects held by the scene manager
		gr::Transform m_transform;
	};
}