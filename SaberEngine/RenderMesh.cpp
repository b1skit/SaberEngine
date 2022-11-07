#include "RenderMesh.h"
#include "Mesh.h"
#include "Transform.h"

using gr::Transform;
using gr::Mesh;
using std::shared_ptr;
using std::vector;

namespace gr
{
	RenderMesh::RenderMesh(Transform* parent, shared_ptr<Mesh> meshPrimitive)
	{
		m_transform.SetParent(parent);
		AddChildMeshPrimitive(meshPrimitive);
	}


	RenderMesh::RenderMesh(RenderMesh const& rhs)
	{
		m_meshPrimitives = rhs.m_meshPrimitives;
		m_transform = rhs.m_transform;
	}


	void RenderMesh::AddChildMeshPrimitive(shared_ptr<gr::Mesh> mesh)
	{
		mesh->GetOwnerTransform() = &m_transform;
		m_meshPrimitives.push_back(mesh);
	}
}
