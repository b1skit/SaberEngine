// © 2023 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CameraComponent.h"
#include "GameplayManager.h"

namespace fr
{
	entt::entity CameraComponent::AttachCameraComponent(
		fr::GameplayManager& gpm, entt::entity sceneNode, gr::Camera::Config const& cameraConfig)
	{
		return entt::null; // TEMP HAX!!!!!!!!!!!!
	}


	gr::Camera::RenderData CameraComponent::CreateRenderData(CameraComponent const& cameraComponent)
	{
		return gr::Camera::RenderData{}; // TEMP HAX!!!!!!!!!!!!
	}
}