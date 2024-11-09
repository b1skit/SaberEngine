// © 2024 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "RenderDataComponent.h"
#include "SkinningComponent.h"
#include "TransformComponent.h"

#include "Core/Util/ImGuiUtils.h"


namespace fr
{
	SkinningComponent& SkinningComponent::AttachSkinningComponent(
		entt::entity owningEntity,
		std::vector<gr::TransformID>&& jointTranformIDs,
		std::vector<glm::mat4>&& inverseBindMatrices,
		gr::TransformID skeletonNodeID)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A SkinningComponent's owningEntity requires a RenderDataComponent");

		SkinningComponent* newSkinningCmpt = 
			em.EmplaceComponent<fr::SkinningComponent>(
				owningEntity, 
				PrivateCTORTag{}, 
				std::move(jointTranformIDs),
				std::move(inverseBindMatrices),
				skeletonNodeID);

		em.EmplaceComponent<DirtyMarker<fr::SkinningComponent>>(owningEntity);

		return *newSkinningCmpt;
	}


	SkinningComponent::SkinningComponent(PrivateCTORTag,
		std::vector<gr::TransformID>&& jointTranformIDs,
		std::vector<glm::mat4>&& inverseBindMatrices,
		gr::TransformID skeletonNodeID)
		: m_jointTransformIDs(std::move(jointTranformIDs))
		, m_inverseBindMatrices(std::move(inverseBindMatrices))
		, m_skeletonNodeID(skeletonNodeID)
	{
	}


	gr::MeshPrimitive::SkinningRenderData SkinningComponent::CreateRenderData(
		entt::entity skinnedMeshPrimitive, SkinningComponent const& skinningComponent)
	{
		return gr::MeshPrimitive::SkinningRenderData{
			.m_jointTransformIDs = skinningComponent.m_jointTransformIDs,
			.m_inverseBindMatrices = skinningComponent.m_inverseBindMatrices,
			.m_skeletonNodeID = skinningComponent.m_skeletonNodeID,
		};
	}


	void SkinningComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningMesh)
	{
		const uint64_t uniqueID = static_cast<uint64_t>(owningMesh);

		fr::SkinningComponent const* skinningCmpt = em.TryGetComponent<fr::SkinningComponent>(owningMesh);

		if (!skinningCmpt)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::CollapsingHeader(std::format("Skin##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			if(skinningCmpt)
			{
				ImGui::Indent();

				// Display the skin metadata:
				std::string const& skeletonNodeIDStr = skinningCmpt->m_skeletonNodeID == gr::k_invalidTransformID ? 
					"<none>" : std::format("{}", skinningCmpt->m_skeletonNodeID);
				ImGui::Text(std::format("Skeleton TransformID: {}", skeletonNodeIDStr).c_str());

				ImGui::Text(std::format("Total inverse bind matrices: {}", 
					skinningCmpt->m_inverseBindMatrices.size()).c_str());

				ImGui::Text(std::format("Total joint transforms: {}", skinningCmpt->m_jointTransformIDs.size()).c_str());

				// Inverse bind matrices:
				if (skinningCmpt->m_inverseBindMatrices.empty())
				{
					ImGui::BeginDisabled();
				}
				if (ImGui::CollapsingHeader(
					std::format("Inverse Bind Matrices##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
				{
					for (size_t i = 0; i < skinningCmpt->m_inverseBindMatrices.size(); ++i)
					{
						util::DisplayMat4x4(std::format("Inverse bind matrix [{}]:", i).c_str(),
							skinningCmpt->m_inverseBindMatrices[i]);
					}
				}
				if (skinningCmpt->m_inverseBindMatrices.empty())
				{
					ImGui::EndDisabled();
				}
				
				// Joints:
				if (ImGui::CollapsingHeader(
					std::format("Joint transform IDs##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
				{
					ImGui::Indent();
					{
						constexpr size_t k_numCols = 10;
						const size_t numRows = skinningCmpt->m_jointTransformIDs.size() % 10;
						ImGui::BeginTable("table1", k_numCols, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendX);
						size_t jointTransformIDIdx = 0;
						bool seenInvalidTransformID = false;
						for (int row = 0; row < numRows; row++)
						{
							ImGui::TableNextRow();
							for (int column = 0; column < k_numCols; column++)
							{
								seenInvalidTransformID |=
									skinningCmpt->m_jointTransformIDs[jointTransformIDIdx] == gr::k_invalidTransformID;

								ImGui::TableNextColumn();

								// Note: We intentionally use the %d specifier here (instead of %u) to keep column sizes smaller
								ImGui::Text("%d", skinningCmpt->m_jointTransformIDs[jointTransformIDIdx++]);

								if (jointTransformIDIdx >= skinningCmpt->m_jointTransformIDs.size())
								{
									break;
								}
							}
							if (jointTransformIDIdx >= skinningCmpt->m_jointTransformIDs.size())
							{
								break;
							}
						}
						ImGui::EndTable();
						if (seenInvalidTransformID)
						{
							ImGui::Text("Note: -1 = Shared default/identity transform");
						}
					}
					ImGui::Unindent();
				}				

				ImGui::Unindent();
			}
		}

		if (!skinningCmpt)
		{
			ImGui::EndDisabled();
		}
	}
}