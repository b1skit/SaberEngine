// © 2024 Adam Badke. All rights reserved.
#include "EntityCommands.h"
#include "EntityManager.h"


namespace pr
{
	pr::EntityManager* IEntityCommand::s_entityManager = nullptr;


	SetMainCameraCommand::SetMainCameraCommand(entt::entity newMainCam)
		: m_newMainCamera(newMainCam)
	{
	}

	void SetMainCameraCommand::Execute(void* cmdData)
	{
		SetMainCameraCommand* cmdPtr = reinterpret_cast<SetMainCameraCommand*>(cmdData);
		s_entityManager->SetMainCamera(cmdPtr->m_newMainCamera);
	}


	SetActiveAmbientLightCommand::SetActiveAmbientLightCommand(entt::entity newActiveAmbient)
		: m_newActiveAmbientLight(newActiveAmbient)
	{
	}

	void SetActiveAmbientLightCommand::Execute(void* cmdData)
	{
		SetActiveAmbientLightCommand* cmdPtr = reinterpret_cast<SetActiveAmbientLightCommand*>(cmdData);
		s_entityManager->SetActiveAmbientLight(cmdPtr->m_newActiveAmbientLight);
	}
}