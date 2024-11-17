// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/MeshPrimitive.h"
#include "Renderer/RenderObjectIDs.h"


namespace fr
{
	class EntityManager;


	class SkinningComponent
	{
	public:
		static SkinningComponent& AttachSkinningComponent(
			entt::entity owningEntity,
			std::vector<gr::TransformID>&& jointTranformIDs,
			std::vector<entt::entity>&& jointEntities,
			std::vector<glm::mat4>&& inverseBindMatrices,
			entt::entity skeletonRootEntity,
			gr::TransformID skeletonTransformID);

		static void UpdateSkinMatrices(fr::EntityManager&, entt::entity owningEntity, SkinningComponent&);

		static gr::MeshPrimitive::SkinningRenderData CreateRenderData(
			entt::entity skinnedMeshPrimitive, SkinningComponent const&);


	public:
		static void ShowImGuiWindow(fr::EntityManager&, entt::entity skinnedEntity);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		SkinningComponent() : SkinningComponent(PrivateCTORTag{}, {}, {}, {}, entt::null, gr::k_invalidTransformID) {}


	public:
		SkinningComponent(PrivateCTORTag, 
			std::vector<gr::TransformID>&& jointTranformIDs,
			std::vector<entt::entity>&& jointEntities,
			std::vector<glm::mat4>&& inverseBindMatrices,
			entt::entity skeletonRootEntity,
			gr::TransformID skeletonTransformID);


	private:
		std::vector<entt::entity> m_jointEntities;
		std::unordered_set<entt::entity> m_jointEntitiesSet; // Initialized once at construction
		
		// Parent of the "common root": The first entity with a TransformComponent NOT part of the skeletal hierarchy
		entt::entity m_parentOfCommonRootEntity; 
		gr::TransformID m_parentOfCommonRootTransformID;

		// Debug: All TransformIDs that might influence a MeshPrimitive: Maps MeshPrimitive joint index to a TransformID
		std::vector<gr::TransformID> m_jointTransformIDs;
		
		// Updated each frame:
		std::vector<glm::mat4> m_jointTransforms; 
		std::vector<glm::mat4> m_transposeInvJointTransforms;

		// Optional: Matrices used to bring coordinates being skinned into the same space as each joint.
		// Matches the order of the m_jointTransformIDs array, with >= the number of joints (if not empty)
		std::vector<glm::mat4> m_inverseBindMatrices; 

		// Optional: Provides a pivot point for skinned geometry
		entt::entity m_skeletonRootEntity;
		gr::TransformID m_skeletonTransformID;
	};
}