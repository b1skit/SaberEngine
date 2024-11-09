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
			std::vector<glm::mat4>&& inverseBindMatrices,
			gr::TransformID skeletonNodeID);

		static gr::MeshPrimitive::SkinningRenderData CreateRenderData(
			entt::entity skinnedMeshPrimitive, SkinningComponent const&);


	public:
		static void ShowImGuiWindow(fr::EntityManager&, entt::entity skinnedEntity);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		SkinningComponent() : SkinningComponent(PrivateCTORTag{}, {}, {}, gr::k_invalidTransformID) {}


	public:
		SkinningComponent(PrivateCTORTag, 
			std::vector<gr::TransformID>&& jointTranformIDs, 
			std::vector<glm::mat4>&& inverseBindMatrices, 
			gr::TransformID skeletonNodeID);


	private:
		// All TransformIDs that might influence a MeshPrimitive.
		// Maps indexes in the MeshPrimitive joints array, to a TransformID
		std::vector<gr::TransformID> m_jointTransformIDs;

		// Optional: Matrices used to bring coordinates being skinned into the same space as each joint.
		// Matches the order of the m_jointTransformIDs array, with >= the number of joints (if not empty)
		std::vector<glm::mat4> m_inverseBindMatrices; 

		// Optional: Provides a pivot point for skinned geometry
		gr::TransformID m_skeletonNodeID;
	};
}