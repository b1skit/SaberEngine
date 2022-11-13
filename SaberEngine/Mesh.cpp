#include "Mesh.h"
#include "MeshPrimitive.h"
#include "Transform.h"

using gr::Transform;
using re::MeshPrimitive;
using std::shared_ptr;
using std::vector;

namespace gr
{
	Mesh::Mesh(gr::Transform* ownerTransform)
		: m_ownerTransform(ownerTransform)
	{
	}


	Mesh::Mesh(Transform* ownerTransform, shared_ptr<re::MeshPrimitive> meshPrimitive)
		: m_ownerTransform(ownerTransform)
	{
		AddMeshPrimitive(meshPrimitive);
	}


	void Mesh::AddMeshPrimitive(shared_ptr<re::MeshPrimitive> meshPrimitive)
	{
		SEAssert("Cannot add a null mesh primitive", meshPrimitive != nullptr);
		SEAssert("Mesh primitive unexpectedly already has an owner transform",
			meshPrimitive->GetOwnerTransform() == nullptr);

		meshPrimitive->GetOwnerTransform() = m_ownerTransform;
		m_meshPrimitives.push_back(meshPrimitive);
	}
}
