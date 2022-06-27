// Renderable interface: For objects drawn by the RenderManager 
#pragma once

#include <vector>
using std::vector;

#include "grMesh.h"


namespace SaberEngine
{
	// Predeclarations:
	class Mesh;
	class Transform;


	class Renderable
	{
	public:
		Renderable() {}
		
		Renderable(vector<gr::Mesh*> viewMeshes)
		{
			m_viewMeshes = viewMeshes;
		}

		~Renderable()
		{

		}

		// Getters/Setters:
		inline vector<gr::Mesh*> const* ViewMeshes() const { return &m_viewMeshes; }

		inline Transform* GetTransform() const { return m_gameObjectTransform; }
		void SetTransform(Transform* transform);

		void AddViewMeshAsChild(gr::Mesh* mesh);


	protected:


	private:
		vector<gr::Mesh*> m_viewMeshes;					// Pointers to statically allocated Mesh objects held by the scene manager
		Transform* m_gameObjectTransform = nullptr;	// The SceneObject that owns this Renderable must set the transform

		/*Mesh* boundsMesh;*/

		/*bool isStatic;*/
	};
}