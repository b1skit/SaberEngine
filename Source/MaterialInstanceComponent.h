// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/Material.h"


namespace fr
{
	class NameComponent;


	class MaterialInstanceComponent
	{
	public:
		static MaterialInstanceComponent& AttachMaterialComponent(
			fr::EntityManager&, entt::entity meshPrimitiveConcept, gr::Material const*);

	public:
		static gr::Material::MaterialInstanceData CreateRenderData(
			MaterialInstanceComponent const&, fr::NameComponent const&);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity owningEntity);


	public:
		bool IsDirty() const;
		void ClearDirtyFlag();


	private:
		gr::Material::MaterialInstanceData m_instanceData;
		bool m_isDirty;
	};

	
	inline bool MaterialInstanceComponent::IsDirty() const
	{
		return m_isDirty;
	}


	inline void MaterialInstanceComponent::ClearDirtyFlag()
	{
		m_isDirty = false;
	}
}