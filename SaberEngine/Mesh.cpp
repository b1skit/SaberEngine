// © 2022 Adam Badke. All rights reserved.
#include "Mesh.h"
#include "MeshPrimitive.h"
#include "Transform.h"

using gr::Transform;
using gr::Bounds;
using re::MeshPrimitive;
using std::shared_ptr;
using std::vector;

namespace gr
{
	Mesh::Mesh(std::string const& name, gr::Transform* ownerTransform)
		: NamedObject(name)
		, m_ownerTransform(ownerTransform)
	{
	}


	Mesh::Mesh(std::string const& name, Transform* ownerTransform, shared_ptr<re::MeshPrimitive> meshPrimitive)
		: NamedObject(name)
		, m_ownerTransform(ownerTransform)
	{
		AddMeshPrimitive(meshPrimitive);
	}


	void Mesh::AddMeshPrimitive(shared_ptr<re::MeshPrimitive> meshPrimitive)
	{
		SEAssert("Cannot add a nullptr MeshPrimitive", meshPrimitive != nullptr);
		m_meshPrimitives.push_back(meshPrimitive);

		m_localBounds.ExpandBounds(
			meshPrimitive->GetBounds().GetTransformedAABBBounds(GetTransform()->GetGlobalMatrix(Transform::TRS)));
	}


	std::vector<std::shared_ptr<re::MeshPrimitive>> const& Mesh::GetMeshPrimitives() const
	{
		return m_meshPrimitives;
	}


	void Mesh::ReplaceMeshPrimitive(size_t index, std::shared_ptr<re::MeshPrimitive> replacement)
	{
		SEAssert("Cannot replace a MeshPrimitive with nullptr", replacement != nullptr);
		SEAssert("Index is out of bounds", index < m_meshPrimitives.size());
		m_meshPrimitives[index] = replacement;

		UpdateBounds();		
	}


	void Mesh::UpdateBounds()
	{
		m_localBounds = Bounds();
		for (shared_ptr<re::MeshPrimitive> meshPrimitive : m_meshPrimitives)
		{
			m_localBounds.ExpandBounds(
				meshPrimitive->GetBounds().GetTransformedAABBBounds(GetTransform()->GetGlobalMatrix(Transform::TRS)));
		}
	}
}