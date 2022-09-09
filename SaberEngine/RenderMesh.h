#pragma once

#include <vector>

#include "Mesh.h"


namespace gr
{
	class Mesh;
	
	// Contains a set of mesh primitives
	class RenderMesh
	{
	public:
		RenderMesh(gr::Transform* gameObjectTransform);
		RenderMesh(RenderMesh const&);
		RenderMesh(RenderMesh&&) = default;
		RenderMesh& operator=(RenderMesh const&) = default;
		~RenderMesh() = default;	

		RenderMesh() = delete;

		// Getters/Setters:
		inline std::vector<std::shared_ptr<gr::Mesh>> const* ViewMeshes() const { return &m_meshPrimitives; }

		inline gr::Transform* GetTransform() const { return m_gameObjectTransform; }
		void SetTransform(gr::Transform* transform);

		void AddChildMeshPrimitive(std::shared_ptr<gr::Mesh> mesh);


	protected:


	private:
		// Pointers to Mesh objects held by the scene manager
		std::vector<std::shared_ptr<gr::Mesh>> m_meshPrimitives; 

		// The SceneObject that owns this RenderMesh must set the transform
		gr::Transform* m_gameObjectTransform;

		/*std::shared_ptr<Mesh> boundsMesh;*/

		/*bool isStatic;*/
	};
}