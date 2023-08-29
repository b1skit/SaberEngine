// © 2022 Adam Badke. All rights reserved.
#include "Mesh.h"
#include "MeshPrimitive.h"
#include "ParameterBlock.h"
#include "Transform.h"

using gr::Transform;
using gr::Bounds;
using re::MeshPrimitive;
using std::shared_ptr;
using std::vector;

namespace gr
{
	std::shared_ptr<re::ParameterBlock> Mesh::CreateInstancedMeshParamsData(gr::Transform* transform)
	{
		std::vector<gr::Transform*> singleTransform(1, transform);
		return CreateInstancedMeshParamsData(singleTransform);
	}


	std::shared_ptr<re::ParameterBlock> Mesh::CreateInstancedMeshParamsData(
		std::vector<gr::Transform*> const& transforms)
	{
		const size_t numInstances = transforms.size();

		std::vector<gr::Mesh::InstancedMeshParams> instancedMeshPBData;
		instancedMeshPBData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshPBData.emplace_back(InstancedMeshParams
				{
					.g_model = transforms[transformIdx]->GetGlobalMatrix(gr::Transform::TRS)
				});
		}

		std::shared_ptr<re::ParameterBlock> instancedMeshParams = re::ParameterBlock::CreateFromArray(
			gr::Mesh::InstancedMeshParams::s_shaderName,
			instancedMeshPBData.data(),
			sizeof(gr::Mesh::InstancedMeshParams),
			numInstances,
			re::ParameterBlock::PBType::SingleFrame);

		return instancedMeshParams;
	}


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


	void Mesh::ShowImGuiWindow()
	{
		ImGui::Text("Name: \"%s\"", GetName().c_str());
		
		const std::string uniqueIDStr = std::to_string(GetUniqueID());

		const std::string transformLabel = "Transform:##" + uniqueIDStr;
		if (ImGui::TreeNode(transformLabel.c_str()))
		{
			m_ownerTransform->ShowImGuiWindow();

			ImGui::TreePop();
		}

		const std::string boundsLabel = "Mesh Bounds:##" + uniqueIDStr;
		if (ImGui::TreeNode(boundsLabel.c_str()))
		{
			m_localBounds.ShowImGuiWindow();

			ImGui::TreePop();
		}

		const std::string meshPrimitivesLabel = 
			std::format("Mesh Primitives ({}):##{}", m_meshPrimitives.size(), uniqueIDStr);
		if (ImGui::TreeNode(meshPrimitivesLabel.c_str()))
		{
			for (size_t i = 0; i < m_meshPrimitives.size(); i++)
			{
				m_meshPrimitives[i]->ShowImGuiWindow();
			}
			ImGui::TreePop();
		}
	}
}