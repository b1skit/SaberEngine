// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"


namespace fr
{
	class NameComponent;


	class MaterialComponent
	{
	public:
		static MaterialComponent& AttachMaterialConcept(
			fr::EntityManager&, entt::entity meshPrimitiveConcept, std::shared_ptr<gr::Material>);

	public:
		static gr::Material::RenderData CreateRenderData(MaterialComponent const&, fr::NameComponent const&);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity owningEntity);


	public:
		// ECS_CONVERSION TODO: Materials are unique, and have their lifecycle managed by the SceneData.
		// But a material component doesn't need to be unique: It just holds pointers to resources held by the
		// SceneData, that could (hypothetically) be changed at runtime. We could treat material components as 
		// instances of a parent material, and allow these copied materials to be modified at runtime.
		//
		// For now, just point to the scene data...
		gr::Material const* m_material;
	};
}