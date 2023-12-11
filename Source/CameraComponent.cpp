// © 2023 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CameraComponent.h"
#include "GameplayManager.h"

namespace fr
{
	void AttachCameraComponent(fr::GameplayManager& gpm, entt::entity entity)
	{
		// TODO....
		// Need to figure out how to handle camera dependencies
		// -> Do we need a name on the renderdata side?
		// -> How to handle the camera Transform?
		//		-> Needs to be updated/dirtied by the GPM -> Which updates/dirties the camera
		
		//gpm.TryEmplaceComponent<gr::Camera>(entity, );
	}
}