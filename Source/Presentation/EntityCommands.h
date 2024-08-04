// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class SetMainCameraCommand
	{
	public:
		SetMainCameraCommand(entt::entity);

		static void Execute(void* cmdData);
		static void Destroy(void*);

	private:
		const entt::entity m_newMainCamera;
	};


	class SetActiveAmbientLightCommand
	{
	public:
		SetActiveAmbientLightCommand(entt::entity);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const entt::entity m_newActiveAmbientLight;
	};
}