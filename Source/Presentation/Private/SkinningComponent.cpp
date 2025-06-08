// © 2024 Adam Badke. All rights reserved.
#include "Private/AnimationComponent.h"
#include "Private/BoundsComponent.h"
#include "Private/EntityManager.h"
#include "Private/MarkerComponents.h"
#include "Private/MeshConcept.h"
#include "Private/MeshPrimitiveComponent.h"
#include "Private/RelationshipComponent.h"
#include "Private/RenderDataComponent.h"
#include "Private/SkinningComponent.h"
#include "Private/TransformComponent.h"

#include "Core/Util/ImGuiUtils.h"


namespace fr
{
	SkinningComponent& SkinningComponent::AttachSkinningComponent(
		entt::entity owningEntity,
		std::vector<gr::TransformID>&& jointTranformIDs,
		std::vector<entt::entity>&& jointEntities,
		std::vector<glm::mat4>&& inverseBindMatrices,
		entt::entity skeletonRootEntity,
		gr::TransformID skeletonTransformID,
		float longestAnimationTimeSec,
		std::vector<entt::entity>&& boundsEntities)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A SkinningComponent's owningEntity requires a RenderDataComponent");

		SEAssert(em.HasComponent<fr::Mesh::MeshConceptMarker>(owningEntity),
			"A SkinningComponent should be attached to the same node as a MeshConceptMarker");

		SkinningComponent* newSkinningCmpt = 
			em.EmplaceComponent<fr::SkinningComponent>(
				owningEntity, 
				PrivateCTORTag{}, 
				std::move(jointTranformIDs),
				std::move(jointEntities),
				std::move(inverseBindMatrices),
				skeletonRootEntity,
				skeletonTransformID,
				longestAnimationTimeSec,
				std::move(boundsEntities));

		em.EmplaceComponent<DirtyMarker<fr::SkinningComponent>>(owningEntity);

		return *newSkinningCmpt;
	}


	SkinningComponent::SkinningComponent(PrivateCTORTag,
		std::vector<gr::TransformID>&& jointTranformIDs,
		std::vector<entt::entity>&& jointEntities,
		std::vector<glm::mat4>&& inverseBindMatrices,
		entt::entity skeletonRootEntity,
		gr::TransformID skeletonTransformID,
		float longestAnimationTimeSec,
		std::vector<entt::entity>&& boundsEntities)
		: m_jointEntities(std::move(jointEntities))
		, m_parentOfCommonRootEntity(entt::null)
		, m_parentOfCommonRootTransformID(gr::k_invalidTransformID)
		, m_jointTransformIDs(std::move(jointTranformIDs))
		, m_inverseBindMatrices(std::move(inverseBindMatrices))
		, m_skeletonRootEntity(skeletonRootEntity)
		, m_skeletonTransformID(skeletonTransformID)
		, m_remainingBoundsUpdatePeriodMs(longestAnimationTimeSec * 1000.f)
		, m_boundsEntities(std::move(boundsEntities))
	{
		m_jointTransforms.resize(m_jointEntities.size(), glm::mat4(1.f));
		m_transposeInvJointTransforms.resize(m_jointEntities.size(), glm::mat4(1.f));

		fr::EntityManager& em = *fr::EntityManager::Get();

		// Find the first entity with a Transform component in the hierarchy above, that is NOT part of the skeletal
		// hierarchy:
		std::unordered_set<entt::entity> jointEntitiesSet;
		jointEntitiesSet.reserve(m_jointEntities.size() + 1);
		for (entt::entity entity : m_jointEntities)
		{
			jointEntitiesSet.emplace(entity);
		}
		if (m_skeletonRootEntity != entt::null)
		{
			jointEntitiesSet.emplace(m_skeletonRootEntity);
		}

		// Find the first parent transform NOT part of our skeletal hierarchy: We'll use this to isolate the skeletal
		// hierarchy from within the transformation hierarchy
		for (entt::entity entity : jointEntitiesSet)
		{
			fr::Relationship const& entityRelationship = em.GetComponent<fr::Relationship>(entity);
			const entt::entity entityParent = entityRelationship.GetParent();
			if (entityParent != entt::null && !jointEntitiesSet.contains(entityParent))
			{
				fr::Relationship const& parentRelationship = em.GetComponent<fr::Relationship>(entityParent);

				entt::entity transformEntity = entt::null;
				if (fr::TransformComponent const* transformCmpt = 
					parentRelationship.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(transformEntity))
				{
					m_parentOfCommonRootEntity = transformEntity;
					m_parentOfCommonRootTransformID = transformCmpt->GetTransformID();

					// If there is an AnimationComponent AT OR ABOVE the m_parentOfCommonRootEntity, we don't want to
					// cancel out its recursive contribution
					entt::entity recursiveRoot = m_parentOfCommonRootEntity;
					fr::Relationship const& curParentRelationship = em.GetComponent<fr::Relationship>(recursiveRoot);
					if (curParentRelationship.GetLastAndEntityInHierarchyAbove<fr::AnimationComponent>(recursiveRoot))
					{
						fr::Relationship const& recursiveRootRelationship = em.GetComponent<fr::Relationship>(recursiveRoot);
						if (recursiveRootRelationship.HasParent())
						{
							fr::Relationship const& nextParentRelationship =
								em.GetComponent<fr::Relationship>(recursiveRootRelationship.GetParent());

							entt::entity parentTransformEntity = entt::null;
							fr::TransformComponent const* parentTransform =
								nextParentRelationship.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(parentTransformEntity);
							if (parentTransform)
							{
								// If the last AnimationComponent in the hierarchy above has a parent, and it has a Transform,
								// that is the actual first matrix we need to cancel
								m_parentOfCommonRootEntity = parentTransformEntity;
								m_parentOfCommonRootTransformID = parentTransform->GetTransformID();
							}
						}
						else // If there is no parent with a TransformComponent, there is nothing to cancel!
						{
							m_parentOfCommonRootEntity = entt::null;
							m_parentOfCommonRootTransformID = gr::k_invalidTransformID;
						}
					}

					// GLTF specs: All nodes in the skeletal hierarchy must have a common root. Thus, we can assume
					// the first node with a parent NOT part of the transformation hierarchy must be the common root,
					// and thus this is its parent					
					break;
				}
			}
		}
	}


	void SkinningComponent::UpdateSkinMatrices(
		fr::EntityManager& em, entt::entity owningEntity, SkinningComponent& skinningCmpt, float deltaTime)
	{
		bool foundDirty = false;

		// As an optimization, we'll use the inverse of the common root's parent transform's global matrix to cancel out
		// any unnecessary matrices in the transformation hierarchy, rather than recompute subranges in the skeletal 
		// hierarchy: i.e. (ABC)^-1 * (ABCDEF) = DEF
		fr::Transform const* parentOfRootTransform = nullptr;
		if (skinningCmpt.m_parentOfCommonRootEntity != entt::null)
		{
			fr::TransformComponent const& parentOfRootTransformCmpt = 
				em.GetComponent<fr::TransformComponent>(skinningCmpt.m_parentOfCommonRootEntity);

			parentOfRootTransform = &parentOfRootTransformCmpt.GetTransform();
		}

		// Combine skin Transforms:
		for (size_t jointIdx = 0; jointIdx < skinningCmpt.m_jointEntities.size(); ++jointIdx)
		{
			const entt::entity curEntity = skinningCmpt.m_jointEntities[jointIdx];

			fr::TransformComponent const* jointTransformCmpt = em.TryGetComponent<fr::TransformComponent>(curEntity);
			
			if (jointTransformCmpt) // If null, no update necessary: Joints are initialized to the identity
			{
				fr::Transform const& jointTransform = jointTransformCmpt->GetTransform();
				if (jointTransform.HasChanged())
				{
					foundDirty = true;

					// Get the joint transform, isolated using the inverse of the root node's parent global transform:
					if (parentOfRootTransform)
					{
						skinningCmpt.m_jointTransforms[jointIdx] = 
							glm::inverse(parentOfRootTransform->GetGlobalMatrix()) * jointTransform.GetGlobalMatrix();
					}
					else
					{
						skinningCmpt.m_jointTransforms[jointIdx] = jointTransform.GetGlobalMatrix();
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
		}

		if (foundDirty)
		{
			em.TryEmplaceComponent<DirtyMarker<fr::SkinningComponent>>(owningEntity);
		}


		// Expand the bounds during the first animation cycle:
		if (skinningCmpt.m_remainingBoundsUpdatePeriodMs > 0.f)
		{
			skinningCmpt.m_remainingBoundsUpdatePeriodMs -= deltaTime;

			for (size_t jointIdx = 0; jointIdx < skinningCmpt.m_jointTransforms.size(); ++jointIdx)
			{
				for (entt::entity boundsEntity : skinningCmpt.m_boundsEntities)
				{
					fr::BoundsComponent& bounds = em.GetComponent<fr::BoundsComponent>(boundsEntity);

					bounds.ExpandBounds(
						(skinningCmpt.m_jointTransforms[jointIdx] * glm::vec4(bounds.GetOriginalMinXYZ(), 1.f)).xyz,
						(skinningCmpt.m_jointTransforms[jointIdx] * glm::vec4(bounds.GetOriginalMaxXYZ(), 1.f)).xyz,
						boundsEntity);
				}
			}
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

				// Parent of the root node:
				std::string const& rootParentTransformIDStr = skinningCmpt->m_parentOfCommonRootTransformID == gr::k_invalidTransformID ?
					"<none>" : std::format("{}", skinningCmpt->m_parentOfCommonRootTransformID);
				ImGui::Text(std::format("Parent of root TransformID: {}", rootParentTransformIDStr).c_str());

				std::string const& rootParentEntity = skinningCmpt->m_parentOfCommonRootEntity == entt::null ?
					"<none>" : std::format("{}", static_cast<uint64_t>(skinningCmpt->m_parentOfCommonRootEntity));
				ImGui::Text(std::format("Parent of root entity: {}", rootParentEntity).c_str());


				ImGui::Separator();


				// Skeleton:
				std::string const& skeletonNodeIDStr = skinningCmpt->m_skeletonTransformID == gr::k_invalidTransformID ?
					"<none>" : std::format("{}", skinningCmpt->m_skeletonTransformID);
				ImGui::Text(std::format("Skeleton TransformID: {}", skeletonNodeIDStr).c_str());

				std::string const& skeletonEntity = skinningCmpt->m_skeletonRootEntity == entt::null ?
					"<none>" : std::format("{}", static_cast<uint64_t>(skinningCmpt->m_skeletonRootEntity));
				ImGui::Text(std::format("Skeleton entity: {}", skeletonEntity).c_str());


				ImGui::Separator();


				// Inverse bind matrices:
				ImGui::Text(std::format("Total inverse bind matrices: {}", 
					skinningCmpt->m_inverseBindMatrices.size()).c_str());
				ImGui::Text(std::format("Total joint transforms: {}", skinningCmpt->m_jointTransformIDs.size()).c_str());

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


				ImGui::Separator();


				// Joints:
				if (ImGui::CollapsingHeader(
					std::format("Joint transform IDs##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
				{
					ImGui::Indent();
					{
						constexpr size_t k_numCols = 10;
						const size_t numRows = std::max(1llu, 
							(skinningCmpt->m_jointTransformIDs.size() / 10) + 
								(skinningCmpt->m_jointTransformIDs.size() % 10 > 0));
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