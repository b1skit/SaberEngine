#pragma once

#include <vector>
#include <memory>

#include "MeshPrimitive.h"

namespace gr
{	
	// TODO: This should be in the fr namespace -> Meshes are gr objects, RenderMeshes are fr objects
	

	// Contains a set of mesh primitives
	class RenderMesh
	{
	public:
		explicit RenderMesh(gr::Transform* parent, std::shared_ptr<gr::MeshPrimitive> meshPrimitive);

		RenderMesh(RenderMesh const&);
		RenderMesh(RenderMesh&&) = default;
		RenderMesh& operator=(RenderMesh const&) = default;
		~RenderMesh() = default;	

		// Getters/Setters:
		inline gr::Transform& GetTransform() { return m_transform; }
		inline gr::Transform const& GetTransform() const { return m_transform; }

		void AddChildMeshPrimitive(std::shared_ptr<gr::MeshPrimitive> meshPrimitive);
		inline std::vector<std::shared_ptr<gr::MeshPrimitive>> const& GetChildMeshPrimitives() const { return m_meshPrimitives; }


	private:
		std::vector<std::shared_ptr<gr::MeshPrimitive>> m_meshPrimitives;
		gr::Transform m_transform;


	private:
		RenderMesh() = delete;
	};
}