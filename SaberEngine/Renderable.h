// Renderable interface: For objects drawn by the RenderManager 
#pragma once

#include <vector>

#include "Mesh.h"


namespace SaberEngine
{
	// Predeclarations:
	class Mesh;
	

	class Renderable
	{
	public:
		Renderable() {}
		
		Renderable(std::vector<std::shared_ptr<gr::Mesh>> viewMeshes)
		{
			m_viewMeshes = viewMeshes;
		}

		~Renderable()
		{

		}

		// Getters/Setters:
		inline std::vector<std::shared_ptr<gr::Mesh>> const* ViewMeshes() const { return &m_viewMeshes; }

		inline gr::Transform* GetTransform() const { return m_gameObjectTransform; }
		void SetTransform(gr::Transform* transform);

		void AddViewMeshAsChild(std::shared_ptr<gr::Mesh> mesh);


	protected:


	private:
		std::vector<std::shared_ptr<gr::Mesh>> m_viewMeshes;					// Pointers to statically allocated Mesh objects held by the scene manager
		gr::Transform* m_gameObjectTransform = nullptr;	// The SceneObject that owns this Renderable must set the transform

		/*std::shared_ptr<Mesh> boundsMesh;*/

		/*bool isStatic;*/
	};
}