#include "Renderable.h"
#include "grMesh.h"
#include "Transform.h"


void SaberEngine::Renderable::SetTransform(Transform* transform)
{
	this->gameObjectTransform = transform;

	// Update the parents of any view meshes
	for (unsigned int i = 0; i < (unsigned int)viewMeshes.size(); i++)
	{
		viewMeshes.at(i)->GetTransform().Parent(this->gameObjectTransform);
	}
}

void SaberEngine::Renderable::AddViewMeshAsChild(gr::Mesh* mesh)
{
	mesh->GetTransform().Parent(this->gameObjectTransform);

	viewMeshes.push_back(mesh);
}
