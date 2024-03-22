// © 2024 Adam Badke. All rights reserved.
#include "EntityCommands.h"
#include "EntityManager.h"


namespace fr
{
	SetMainCameraCommand::SetMainCameraCommand(entt::entity newMainCam)
		: m_newMainCamera(newMainCam)
	{
	}

	void SetMainCameraCommand::Execute(void* cmdData)
	{
		SetMainCameraCommand* cmdPtr = reinterpret_cast<SetMainCameraCommand*>(cmdData);
		fr::EntityManager::Get()->SetMainCamera(cmdPtr->m_newMainCamera);
	}

	void SetMainCameraCommand::Destroy(void* cmdData)
	{
		SetMainCameraCommand* cmdPtr = reinterpret_cast<SetMainCameraCommand*>(cmdData);
		cmdPtr->~SetMainCameraCommand();
	}


	SetActiveAmbientLightCommand::SetActiveAmbientLightCommand(entt::entity newActiveAmbient)
		: m_newActiveAmbientLight(newActiveAmbient)
	{
	}

	void SetActiveAmbientLightCommand::Execute(void* cmdData)
	{
		SetActiveAmbientLightCommand* cmdPtr = reinterpret_cast<SetActiveAmbientLightCommand*>(cmdData);
		fr::EntityManager::Get()->SetActiveAmbientLight(cmdPtr->m_newActiveAmbientLight);
	}

	void SetActiveAmbientLightCommand::Destroy(void* cmdData)
	{
		SetActiveAmbientLightCommand* cmdPtr = reinterpret_cast<SetActiveAmbientLightCommand*>(cmdData);
		cmdPtr->~SetActiveAmbientLightCommand();
	}
}