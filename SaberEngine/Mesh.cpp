#include "Mesh.h"
#include "MeshPrimitive.h"
#include "Transform.h"

using gr::Transform;
using re::MeshPrimitive;
using std::shared_ptr;
using std::vector;

namespace gr
{
	Mesh::Mesh(Transform* parent, shared_ptr<re::MeshPrimitive> meshPrimitive)
	{
		m_transform.SetParent(parent);
		AddMeshPrimitive(meshPrimitive);
	}


	Mesh::Mesh(Mesh const& rhs)
	{
		m_meshPrimitives = rhs.m_meshPrimitives;
		m_transform = rhs.m_transform;
	}


	void Mesh::AddMeshPrimitive(shared_ptr<re::MeshPrimitive> meshPrimitive)
	{
		meshPrimitive->GetOwnerTransform() = &m_transform;
		m_meshPrimitives.push_back(meshPrimitive);
	}
}
