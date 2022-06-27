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
			this->viewMeshes = viewMeshes;
		}

		~Renderable()
		{

		}

		// Getters/Setters:
		inline vector<gr::Mesh*> const* ViewMeshes() const { return &viewMeshes; }

		inline Transform* GetTransform() const { return gameObjectTransform; }
		void SetTransform(Transform* transform);

		void AddViewMeshAsChild(gr::Mesh* mesh);


	protected:


	private:
		vector<gr::Mesh*> viewMeshes;					// Pointers to statically allocated Mesh objects held by the scene manager
		Transform* gameObjectTransform = nullptr;	// The SceneObject that owns this Renderable must set the transform

		/*Mesh* boundsMesh;*/

		/*bool isStatic;*/
	};
}