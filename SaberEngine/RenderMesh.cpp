#include "RenderMesh.h"
#include "MeshPrimitive.h"
#include "Transform.h"

using gr::Transform;
using gr::MeshPrimitive;
using std::shared_ptr;
using std::vector;

namespace gr
{
	RenderMesh::RenderMesh(Transform* parent, shared_ptr<MeshPrimitive> meshPrimitive)
	{
		m_transform.SetParent(parent);
		AddChildMeshPrimitive(meshPrimitive);
	}


	RenderMesh::RenderMesh(RenderMesh const& rhs)
	{
		m_meshPrimitives = rhs.m_meshPrimitives;
		m_transform = rhs.m_transform;
	}


	void RenderMesh::AddChildMeshPrimitive(shared_ptr<gr::MeshPrimitive> meshPrimitive)
	{
		meshPrimitive->GetOwnerTransform() = &m_transform;
		m_meshPrimitives.push_back(meshPrimitive);
	}
}
