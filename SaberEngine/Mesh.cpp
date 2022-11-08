#include "Mesh.h"
#include "MeshPrimitive.h"
#include "Transform.h"

using gr::Transform;
using gr::MeshPrimitive;
using std::shared_ptr;
using std::vector;

namespace gr
{
	Mesh::Mesh(Transform* parent, shared_ptr<MeshPrimitive> meshPrimitive)
	{
		m_transform.SetParent(parent);
		AddMeshPrimitive(meshPrimitive);
	}


	Mesh::Mesh(Mesh const& rhs)
	{
		m_meshPrimitives = rhs.m_meshPrimitives;
		m_transform = rhs.m_transform;
	}


	void Mesh::AddMeshPrimitive(shared_ptr<gr::MeshPrimitive> meshPrimitive)
	{
		meshPrimitive->GetOwnerTransform() = &m_transform;
		m_meshPrimitives.push_back(meshPrimitive);
	}
}
