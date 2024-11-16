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
		std::vector<entt::entity>&& jointEntities,
		std::vector<glm::mat4>&& inverseBindMatrices,
		entt::entity skeletonRootEntity,
		gr::TransformID skeletonTransformID)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A SkinningComponent's owningEntity requires a RenderDataComponent");

		SkinningComponent* newSkinningCmpt = 
			em.EmplaceComponent<fr::SkinningComponent>(
				owningEntity, 
				PrivateCTORTag{}, 
				std::move(jointTranformIDs),
				std::move(jointEntities),
				std::move(inverseBindMatrices),
				skeletonRootEntity,
				skeletonTransformID);

		em.EmplaceComponent<DirtyMarker<fr::SkinningComponent>>(owningEntity);

		return *newSkinningCmpt;
	}


	SkinningComponent::SkinningComponent(PrivateCTORTag,
		std::vector<gr::TransformID>&& jointTranformIDs,
		std::vector<entt::entity>&& jointEntities,
		std::vector<glm::mat4>&& inverseBindMatrices,
		entt::entity skeletonRootEntity,
		gr::TransformID skeletonTransformID)
		: m_jointEntities(std::move(jointEntities))
		, m_jointTransformIDs(std::move(jointTranformIDs))
		, m_inverseBindMatrices(std::move(inverseBindMatrices))
		, m_skeletonRootEntity(skeletonRootEntity)
		, m_skeletonTransformID(skeletonTransformID)
	{
		m_jointTransforms.resize(m_jointEntities.size(), glm::mat4(1.f));
		m_transposeInvJointTransforms.resize(m_jointEntities.size(), glm::mat4(1.f));

		// Create a set so we can quickly query any entities in the skeleton
		m_jointEntitiesSet.reserve(m_jointEntities.size() + 1);
		for (entt::entity entity : m_jointEntities)
		{
			m_jointEntitiesSet.emplace(entity);
		}
		if (m_skeletonRootEntity != entt::null)
		{
			m_jointEntitiesSet.emplace(m_skeletonRootEntity);
		}
	}


	void SkinningComponent::UpdateSkinMatrices(
		fr::EntityManager& em, entt::entity owningEntity, SkinningComponent& skinningCmpt)
	{
		bool foundDirty = false;

		// Combine skin Transforms:
		for (size_t jointIdx = 0; jointIdx < skinningCmpt.m_jointEntities.size(); ++jointIdx)
		{
			const entt::entity curEntity = skinningCmpt.m_jointEntities[jointIdx];

			fr::TransformComponent const* jointTransformCmpt = em.TryGetComponent<fr::TransformComponent>(curEntity);
			
			if (jointTransformCmpt) // If null, no update necessary: Joints are initialized to the identity
			{
				fr::Transform const& jointTransform = jointTransformCmpt->GetTransform();
				foundDirty |= jointTransform.HasChanged();

				skinningCmpt.m_jointTransforms[jointIdx] = jointTransform.GetLocalMatrix();

				// Combine all the ancestors
				fr::Relationship const& jointEntityRelationship = em.GetComponent<fr::Relationship>(curEntity);
				const entt::entity parentEntity = jointEntityRelationship.GetParent();
				if (parentEntity != entt::null)
				{
					entt::entity nextTransformEntity = entt::null;
					fr::TransformComponent const* nextTransformCmpt =
						em.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(parentEntity, nextTransformEntity);

					while (nextTransformEntity != entt::null &&
						skinningCmpt.m_jointEntitiesSet.contains(nextTransformEntity))
					{
						SEAssert(nextTransformCmpt, "Next transform component is null. This is unexpected");

						fr::Transform const* nextTransform = &nextTransformCmpt->GetTransform();
						foundDirty |= nextTransform->HasChanged();

						skinningCmpt.m_jointTransforms[jointIdx] =
							nextTransform->GetLocalMatrix() * skinningCmpt.m_jointTransforms[jointIdx];

						fr::Relationship const& nextTransformRelationship =
							em.GetComponent<fr::Relationship>(nextTransformEntity);

						const entt::entity nextParentEntity = nextTransformRelationship.GetParent();
						if (nextParentEntity != entt::null)
						{
							nextTransformCmpt = em.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(
								nextParentEntity, nextTransformEntity);
						}
						else
						{
							break;
						}
					}
				}

				// Inverse bind matrix
				if (!skinningCmpt.m_inverseBindMatrices.empty())
				{
					skinningCmpt.m_jointTransforms[jointIdx] =
						skinningCmpt.m_jointTransforms[jointIdx] * skinningCmpt.m_inverseBindMatrices[jointIdx];
				}

				// Transpose inverse:
				skinningCmpt.m_transposeInvJointTransforms[jointIdx] =
					glm::transpose(glm::inverse(skinningCmpt.m_jointTransforms[jointIdx]));
			}
		}

		if (foundDirty)
		{
			em.TryEmplaceComponent<DirtyMarker<fr::SkinningComponent>>(owningEntity);
		}
	}


	gr::MeshPrimitive::SkinningRenderData SkinningComponent::CreateRenderData(
		entt::entity skinnedMeshPrimitive, SkinningComponent const& skinningCmpt)
	{
		return gr::MeshPrimitive::SkinningRenderData{
			.m_jointTransforms = skinningCmpt.m_jointTransforms,
			.m_transposeInvJointTransforms = skinningCmpt.m_transposeInvJointTransforms,
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
				std::string const& skeletonNodeIDStr = skinningCmpt->m_skeletonTransformID == gr::k_invalidTransformID ?
					"<none>" : std::format("{}", skinningCmpt->m_skeletonTransformID);
				ImGui::Text(std::format("Skeleton TransformID: {}", skeletonNodeIDStr).c_str());

				std::string const& skeltonEntity = skinningCmpt->m_skeletonRootEntity == entt::null ?
					"<none>" : std::format("{}", static_cast<uint64_t>(skinningCmpt->m_skeletonRootEntity));
				ImGui::Text(std::format("Skeleton entity: {}", skeltonEntity).c_str());

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