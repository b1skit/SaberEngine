// © 2022 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "ImGuiUtils.h"
#include "Mesh.h"
#include "MeshPrimitive.h"
#include "ParameterBlock.h"
#include "Relationship.h"
#include "RenderDataComponent.h"
#include "Transform.h"
#include "TransformComponent.h"

using gr::Transform;
using fr::Bounds;
using gr::MeshPrimitive;
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
		const uint32_t numInstances = static_cast<uint32_t>(transforms.size());

		std::vector<gr::Mesh::InstancedMeshParams> instancedMeshPBData;
		instancedMeshPBData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshPBData.emplace_back(InstancedMeshParams
				{
					.g_model = transforms[transformIdx]->GetGlobalMatrix(),
					.g_transposeInvModel = 
						glm::transpose(glm::inverse(transforms[transformIdx]->GetGlobalMatrix()))
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


	std::shared_ptr<re::ParameterBlock> Mesh::CreateInstancedMeshParamsData(
		glm::mat4 const* model, glm::mat4* transposeInvModel)
	{
		gr::Mesh::InstancedMeshParams instancedMeshPBData;

		instancedMeshPBData.g_model = model ? *model : glm::mat4(1.f);
		instancedMeshPBData.g_transposeInvModel = transposeInvModel ? *transposeInvModel : glm::mat4(1.f);

		return re::ParameterBlock::CreateFromArray(
			gr::Mesh::InstancedMeshParams::s_shaderName,
			&instancedMeshPBData,
			sizeof(gr::Mesh::InstancedMeshParams),
			1,
			re::ParameterBlock::PBType::SingleFrame);
	}


	void Mesh::AttachMeshConcept(fr::GameplayManager& gpm, entt::entity sceneNode, uint32_t expectedNumPrimitives)
	{
		SEAssert("A mesh requires a transform. The scene node should have attached this already",
			gpm.HasComponent<fr::TransformComponent>(sceneNode));

		fr::Bounds::AttachBoundsComponent(gpm, sceneNode); // Mesh bounds: Encompasses all attached primitive bounds
		
		fr::Relationship& meshRelationship = fr::Relationship::AttachRelationshipComponent(gpm, sceneNode);

		gr::RenderDataComponent& renderDataComponent = 
			gr::RenderDataComponent::AttachRenderDataComponent(gpm, sceneNode, expectedNumPrimitives);
	}


	Mesh::Mesh(std::string const& name, gr::Transform* ownerTransform)
		: m_name(name)
		, m_ownerTransform(ownerTransform)
	{
	}


	Mesh::Mesh(std::string const& name, Transform* ownerTransform, shared_ptr<gr::MeshPrimitive> meshPrimitive)
		: m_name(name)
		, m_ownerTransform(ownerTransform)
	{
		AddMeshPrimitive(meshPrimitive);
	}


	void Mesh::AddMeshPrimitive(shared_ptr<gr::MeshPrimitive> meshPrimitive)
	{
		SEAssert("Cannot add a nullptr MeshPrimitive", meshPrimitive != nullptr);
		m_meshPrimitives.push_back(meshPrimitive);

		m_localBounds.ExpandBounds(meshPrimitive->GetBounds());
	}


	std::vector<std::shared_ptr<gr::MeshPrimitive>> const& Mesh::GetMeshPrimitives() const
	{
		return m_meshPrimitives;
	}


	void Mesh::ReplaceMeshPrimitive(size_t index, std::shared_ptr<gr::MeshPrimitive> replacement)
	{
		SEAssert("Cannot replace a MeshPrimitive with nullptr", replacement != nullptr);
		SEAssert("Index is out of bounds", index < m_meshPrimitives.size());
		m_meshPrimitives[index] = replacement;

		// ECS_CONVERSION TODO: WHEN WE REMOVE THE BOUNDS DURING THE ECS CONVERSION, WE'LL NEED TO HANDLE UPDATING THE
		// BOUNDS COMPONENT IF A MESH PRIMITIVE CHANGES
		UpdateBounds();
	}


	void Mesh::UpdateBounds()
	{
		m_localBounds = Bounds();
		for (shared_ptr<gr::MeshPrimitive> meshPrimitive : m_meshPrimitives)
		{
			m_localBounds.ExpandBounds(meshPrimitive->GetBounds());
		}
	}


	void Mesh::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(
			std::format("{}##{}", GetName(), util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			const std::string uniqueIDStr = std::to_string(util::PtrToID(this));

			if (ImGui::CollapsingHeader(
				std::format("Transform:##{}", util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				m_ownerTransform->ShowImGuiWindow();
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader(
				std::format("Mesh Bounds:##{}", util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				m_localBounds.ShowImGuiWindow();
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader(
				std::format("Mesh Primitives ({}):##{}", m_meshPrimitives.size(), util::PtrToID(this)).c_str(), 
				ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_meshPrimitives.size(); i++)
				{
					m_meshPrimitives[i]->ShowImGuiWindow();
				}
				ImGui::Unindent();
			}
			ImGui::Unindent();
		}
	}
}