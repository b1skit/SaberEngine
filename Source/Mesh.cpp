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


namespace fr
{
	entt::entity Mesh::CreateMeshConcept(entt::entity sceneNode, char const* name)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("A Mesh concept requires a Transform via a SceneNode. The sceneNode should have this already",
			gpm.HasComponent<fr::TransformComponent>(sceneNode));
		SEAssert("A mesh requires a Relationship with a SceneNode. The sceneNode parent should have this already",
			gpm.HasComponent<fr::Relationship>(sceneNode));

		entt::entity meshEntity = gpm.CreateEntity(name);

		gpm.EmplaceComponent<fr::Mesh::MeshConceptMarker>(meshEntity);

		fr::TransformComponent const& transformComponent = gpm.GetComponent<fr::TransformComponent>(sceneNode);

		gr::RenderDataComponent::AttachNewRenderDataComponent(gpm, meshEntity, transformComponent.GetTransformID());

		fr::Bounds::AttachBoundsComponent(gpm, meshEntity); // Mesh bounds: Encompasses all attached primitive bounds

		fr::Relationship& meshRelationship = fr::Relationship::AttachRelationshipComponent(gpm, meshEntity);
		meshRelationship.SetParent(gpm, sceneNode);

		return meshEntity;
	}
}


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


	std::shared_ptr<re::ParameterBlock> Mesh::CreateInstancedMeshParamsData(
		fr::TransformComponent::RenderData const& renderData)
	{
		gr::Mesh::InstancedMeshParams instancedMeshPBData{
			.g_model = renderData.g_model,
			.g_transposeInvModel = renderData.g_transposeInvModel
		};

		return re::ParameterBlock::CreateFromArray(
			gr::Mesh::InstancedMeshParams::s_shaderName,
			&instancedMeshPBData,
			sizeof(gr::Mesh::InstancedMeshParams),
			1,
			re::ParameterBlock::PBType::SingleFrame);
	}


	std::shared_ptr<re::ParameterBlock> Mesh::CreateInstancedMeshParamsData(
		std::vector<fr::TransformComponent::RenderData const*> const& transformRenderData)
	{
		const uint32_t numInstances = static_cast<uint32_t>(transformRenderData.size());

		std::vector<gr::Mesh::InstancedMeshParams> instancedMeshPBData;
		instancedMeshPBData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshPBData.emplace_back(InstancedMeshParams
				{
					.g_model = transformRenderData[transformIdx]->g_model,
					.g_transposeInvModel = transformRenderData[transformIdx]->g_transposeInvModel
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





	// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!


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
		// ECS_CONVERSION TODO: Restore ImGui functionality!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


		//if (ImGui::CollapsingHeader(
		//	std::format("{}##{}", GetName(), util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
		//{
		//	ImGui::Indent();
		//	const std::string uniqueIDStr = std::to_string(util::PtrToID(this));

		//	if (ImGui::CollapsingHeader(
		//		std::format("Transform:##{}", util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
		//	{
		//		ImGui::Indent();
		//		m_ownerTransform->ShowImGuiWindow();
		//		ImGui::Unindent();
		//	}

		//	if (ImGui::CollapsingHeader(
		//		std::format("Mesh Bounds:##{}", util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
		//	{
		//		ImGui::Indent();
		//		m_localBounds.ShowImGuiWindow();
		//		ImGui::Unindent();
		//	}

		//	if (ImGui::CollapsingHeader(
		//		std::format("Mesh Primitives ({}):##{}", m_meshPrimitives.size(), util::PtrToID(this)).c_str(), 
		//		ImGuiTreeNodeFlags_None))
		//	{
		//		ImGui::Indent();
		//		for (size_t i = 0; i < m_meshPrimitives.size(); i++)
		//		{
		//			m_meshPrimitives[i]->ShowImGuiWindow();
		//		}
		//		ImGui::Unindent();
		//	}
		//	ImGui::Unindent();
		//}
	}
}