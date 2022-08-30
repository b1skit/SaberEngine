#include "Renderable.h"
#include "Mesh.h"
#include "Transform.h"

using gr::Transform;


void SaberEngine::Renderable::SetTransform(Transform* transform)
{
	m_gameObjectTransform = transform;

	// Update the parents of any view meshes
	for (unsigned int i = 0; i < (unsigned int)m_viewMeshes.size(); i++)
	{
		m_viewMeshes.at(i)->GetTransform().SetParent(m_gameObjectTransform);
	}
}

void SaberEngine::Renderable::AddViewMeshAsChild(std::shared_ptr<gr::Mesh> mesh)
{
	mesh->GetTransform().SetParent(m_gameObjectTransform);

	m_viewMeshes.push_back(mesh);
}
