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
		m_meshPrimitives.push_back(meshPrimitive);
	}


	std::vector<std::shared_ptr<re::MeshPrimitive>> const& Mesh::GetMeshPrimitives() const
	{
		return m_meshPrimitives;
	}


	void Mesh::ReplaceMeshPrimitive(size_t index, std::shared_ptr<re::MeshPrimitive> replacement)
	{
		SEAssert("Index is out of bounds", index < m_meshPrimitives.size());
		m_meshPrimitives[index] = replacement;
	}
}