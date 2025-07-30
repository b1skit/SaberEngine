// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace pr
{
	class EntityManager;


	class IEntityCommand
	{
	protected:
		friend class pr::EntityManager;
		static pr::EntityManager* s_entityManager;
	};


	class SetMainCameraCommand final : public virtual IEntityCommand
	{
	public:
		SetMainCameraCommand(entt::entity);

		static void Execute(void* cmdData);

	private:
		const entt::entity m_newMainCamera;
	};


	class SetActiveAmbientLightCommand final : public virtual IEntityCommand
	{
	public:
		SetActiveAmbientLightCommand(entt::entity);

		static void Execute(void*);

	private:
		const entt::entity m_newActiveAmbientLight;
	};
}