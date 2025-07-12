// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace pr
{
	class SetMainCameraCommand final
	{
	public:
		SetMainCameraCommand(entt::entity);

		static void Execute(void* cmdData);
		static void Destroy(void*);

	private:
		const entt::entity m_newMainCamera;
	};


	class SetActiveAmbientLightCommand final
	{
	public:
		SetActiveAmbientLightCommand(entt::entity);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const entt::entity m_newActiveAmbientLight;
	};
}