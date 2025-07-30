// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/Material.h"


namespace core
{
	template<typename T>
	class InvPtr;
}

namespace pr
{
	class NameComponent;


	class MaterialInstanceComponent final
	{
	public:
		static MaterialInstanceComponent& AttachMaterialComponent(
			pr::EntityManager&, entt::entity meshPrimitiveConcept, core::InvPtr<gr::Material> const&);

	public:
		static gr::Material::MaterialInstanceRenderData CreateRenderData(
			pr::EntityManager&, entt::entity, MaterialInstanceComponent const&);

		static void ShowImGuiWindow(pr::EntityManager&, entt::entity owningEntity);


	public:
		bool IsDirty() const;
		void ClearDirtyFlag();
		core::InvPtr<gr::Material> const& GetMaterial() const;


	private:
		gr::Material::MaterialInstanceRenderData m_instanceData;
		const core::InvPtr<gr::Material> m_srcMaterial;
		bool m_isDirty;


	private:
		struct PrivateCTORTag {};
		MaterialInstanceComponent() = delete;
	public:
		MaterialInstanceComponent(PrivateCTORTag, core::InvPtr<gr::Material> const&);
	};

	
	inline bool MaterialInstanceComponent::IsDirty() const
	{
		return m_isDirty;
	}


	inline void MaterialInstanceComponent::ClearDirtyFlag()
	{
		m_isDirty = false;
	}


	inline core::InvPtr<gr::Material> const& MaterialInstanceComponent::GetMaterial()  const
	{
		return m_srcMaterial;
	}
}