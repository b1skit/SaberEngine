// © 2023 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CameraComponent.h"
#include "GameplayManager.h"

namespace fr
{
	entt::entity CreatePlayerCameraEntity(fr::GameplayManager& gpm)
	{
		// TODO...
		return entt::null;
	}

	//void AttachCameraComponent(fr::GameplayManager& gpm, entt::entity entity)
	//{
	//	// TODO....
	//	// Need to figure out how to handle camera dependencies
	//	// -> Do we need a name on the renderdata side?
	//	// -> How to handle the camera Transform?
	//	//		-> Needs to be updated/dirtied by the GPM -> Which updates/dirties the camera

	//	// Strategy:
	//	// The gr::Camera object IS the component
	//	// We push the CameraParams PB data to the render command queue
	//	
	//	gpm.TryEmplaceComponent<gr::Camera::CameraParams>(entity, );
	//}
}