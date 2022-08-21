// Renderable interface: For objects drawn by the RenderManager 
#pragma once

#include <vector>
using std::vector;

#include "Mesh.h"


namespace SaberEngine
{
	// Predeclarations:
	class Mesh;
	class Transform;


	class Renderable
	{
	public:
		Renderable() {}
		
		Renderable(vector<std::shared_ptr<gr::Mesh>> viewMeshes)
		{
			m_viewMeshes = viewMeshes;
		}

		~Renderable()
		{

		}

		// Getters/Setters:
		inline vector<std::shared_ptr<gr::Mesh>> const* ViewMeshes() const { return &m_viewMeshes; }

		inline Transform* GetTransform() const { return m_gameObjectTransform; }
		void SetTransform(Transform* transform);

		void AddViewMeshAsChild(std::shared_ptr<gr::Mesh> mesh);


	protected:


	private:
		vector<std::shared_ptr<gr::Mesh>> m_viewMeshes;					// Pointers to statically allocated Mesh objects held by the scene manager
		Transform* m_gameObjectTransform = nullptr;	// The SceneObject that owns this Renderable must set the transform

		/*std::shared_ptr<Mesh> boundsMesh;*/

		/*bool isStatic;*/
	};
}