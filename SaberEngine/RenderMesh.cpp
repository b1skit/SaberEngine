#include "RenderMesh.h"
#include "Mesh.h"
#include "Transform.h"

using gr::Transform;
using gr::Mesh;
using std::shared_ptr;
using std::vector;

namespace gr
{
	RenderMesh::RenderMesh(Transform* gameObjectTransform)
	{
		SetTransform(gameObjectTransform);
	}


	RenderMesh::RenderMesh(RenderMesh const& rhs)
	{
		m_meshPrimitives = rhs.m_meshPrimitives;
		SetTransform(rhs.m_gameObjectTransform);
	}


	void RenderMesh::SetTransform(Transform* transform)
	{
		m_gameObjectTransform = transform;

		// Update the parents of any view meshes
		for (size_t i = 0; i < m_meshPrimitives.size(); i++)
		{
			m_meshPrimitives.at(i)->GetTransform().SetParent(m_gameObjectTransform);
		}
	}


	void RenderMesh::AddChildMeshPrimitive(shared_ptr<gr::Mesh> mesh)
	{
		mesh->GetTransform().SetParent(m_gameObjectTransform);
		m_meshPrimitives.push_back(mesh);
	}
}
