// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"


namespace fr
{
	class Material
	{
	public:
		struct MaterialComponent
		{
			// ECS_CONVERSION TODO: Materials are unique, and have their lifecycle managed by the SceneData.
			// But a material component doesn't need to be unique: It just holds pointers to resources held by the
			// SceneData, that could (hypothetically) be changed at runtime. We could treat material components as 
			// instances of a parent material, and allow these copied materials to be modified at runtime.
			//
			// For now, just point to the scene data...
			gr::Material const* m_material;
		};


	public:
		static gr::Material::RenderData CreateRenderData(MaterialComponent const&);


	public:
		static MaterialComponent& AttachMaterialConcept(
			entt::entity meshPrimitiveConcept, std::shared_ptr<gr::Material>);
	};
}