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
			entt::entity owningEntity, std::vector<gr::TransformID>&& jointTranformIDs);

		static gr::MeshPrimitive::SkinningRenderData CreateRenderData(
			entt::entity skinnedMeshPrimitive, SkinningComponent const&);


	public:
		static void ShowImGuiWindow(fr::EntityManager&, entt::entity skinnedEntity);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		SkinningComponent() : SkinningComponent(PrivateCTORTag{}, {}) {}


	public:
		SkinningComponent(PrivateCTORTag, std::vector<gr::TransformID>&& jointTranformIDs);


	private:
		// All TransformIDs that might influence a MeshPrimitive.
		// Maps indexes in the MeshPrimitive joints array, to a TransformID
		std::vector<gr::TransformID> m_jointTransformIDs;
	};
}