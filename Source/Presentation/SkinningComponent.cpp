// © 2024 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "RenderDataComponent.h"
#include "SkinningComponent.h"

#include "Core/Util/ImGuiUtils.h"


namespace fr
{
	SkinningComponent& SkinningComponent::AttachSkinningComponent(
		entt::entity owningEntity, std::vector<gr::TransformID>&& jointTranformIDs)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A SkinningComponent's owningEntity requires a RenderDataComponent");

		SkinningComponent* newSkinningCmpt = 
			em.EmplaceComponent<fr::SkinningComponent>(owningEntity, PrivateCTORTag{}, std::move(jointTranformIDs));

		em.EmplaceComponent<DirtyMarker<fr::SkinningComponent>>(owningEntity);

		return *newSkinningCmpt;
	}


	SkinningComponent::SkinningComponent(PrivateCTORTag, std::vector<gr::TransformID>&& jointTranformIDs)
		: m_jointTransformIDs(std::move(jointTranformIDs))
	{
	}


	gr::MeshPrimitive::SkinningRenderData SkinningComponent::CreateRenderData(
		entt::entity skinnedMeshPrimitive, SkinningComponent const& skinningComponent)
	{
		return gr::MeshPrimitive::SkinningRenderData{
			.m_jointTransformIDs = skinningComponent.m_jointTransformIDs,
		};
	}


	void SkinningComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity skinnedEntity)
	{
		const uint64_t uniqueID = static_cast<uint64_t>(skinnedEntity);

		fr::SkinningComponent const* skinningCmpt = em.TryGetComponent<fr::SkinningComponent>(skinnedEntity);

		if (!skinningCmpt)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::CollapsingHeader(std::format("Skinning##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			if(skinningCmpt)
			{
				ImGui::Indent();

				ImGui::Text(std::format("Total joint transforms: {}", skinningCmpt->m_jointTransformIDs.size()).c_str());

				std::string transformIDs = "Joint TransformIDs: ";
				for (size_t i = 0; i < skinningCmpt->m_jointTransformIDs.size(); ++i)
				{
					transformIDs += std::format("{}{}",
						skinningCmpt->m_jointTransformIDs[i],
						i == skinningCmpt->m_jointTransformIDs.size() - 1 ? "" : ", ");
				}
				ImGui::Text(transformIDs.c_str());

				ImGui::Unindent();
			}
		}

		if (!skinningCmpt)
		{
			ImGui::EndDisabled();
		}
	}
}