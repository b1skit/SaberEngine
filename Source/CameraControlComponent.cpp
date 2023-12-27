// © 2022 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "Config.h"
#include "GameplayManager.h"
#include "InputManager.h"
#include "CameraControlComponent.h"
#include "TransformComponent.h"


namespace
{
	constexpr char const* k_playerObjectName = "Player Object";
}

namespace fr
{
	entt::entity CameraControlComponent::CreatePlayerObjectConcept(GameplayManager& gpm, entt::entity cameraConcept)
	{
		entt::entity playerEntity = gpm.CreateEntity(k_playerObjectName);

		gpm.EmplaceComponent<CameraControlComponent>(playerEntity);

		fr::TransformComponent& playerTransform = 
			fr::TransformComponent::AttachTransformComponent(gpm, playerEntity, nullptr);

		// Parent the camera to the player object:
		SetCamera(gpm, playerEntity, cameraConcept);
		
		return playerEntity;
	}


	void CameraControlComponent::SetCamera(
		GameplayManager& gpm, entt::entity camControlEntity, entt::entity cameraConcept)
	{
		SEAssert("camControlEntity entity must have a TransformComponent attached",
			gpm.HasComponent<fr::TransformComponent>(camControlEntity));

		SEAssert("cameraConcept entity must have a CameraComponent attached",
			gpm.HasComponent<fr::CameraComponent>(cameraConcept));

		SEAssert("CameraControlComponent camera requires a TransformComponent",
			gpm.IsInHierarchyAbove<fr::TransformComponent>(cameraConcept));

		fr::Transform& playerTransform = gpm.GetComponent<fr::TransformComponent>(camControlEntity).GetTransform();
		
		fr::Transform& cameraTransform = 
			gpm.GetFirstInHierarchyAbove<fr::TransformComponent>(cameraConcept)->GetTransform();

		// The PlayerObject and Camera must be located at the same point. To avoid stomping imported Camera locations,
		// we move the PlayerObject to the camera. Then, we re-parent the Camera's Transform, to maintain its global
		// orientation but update its local orientation under the PlayerObject Transform
		playerTransform.SetGlobalPosition(cameraTransform.GetGlobalPosition());
		cameraTransform.ReParent(&playerTransform);

		// Note: We don't set a Relationship between the camera controller and a camera; A camera will already have been
		// parented to a SceneNode concept (and thus uses its transform).
	}


	void CameraControlComponent::Update(
		CameraControlComponent& playerObject, 
		fr::TransformComponent& playerTransformCmpt,
		fr::CameraComponent const& cameraCmpt,
		fr::TransformComponent& cameraTransformCmpt,
		double stepTimeMs)
	{
		fr::Transform& playerTransform = playerTransformCmpt.GetTransform();

		fr::Camera const& camera = cameraCmpt.GetCamera();
		fr::Transform& cameraTransform = cameraTransformCmpt.GetTransform();

		SEAssert("Camera transform must be parented to the player transform", 
			cameraTransform.GetParent() == &playerTransform);

		// Reset the cam back to the saved position
		if (en::InputManager::GetMouseInputState(en::InputMouse_Left))
		{
			playerTransform.SetLocalPosition(playerObject.m_savedPosition);
			cameraTransform.SetLocalRotation(glm::vec3(playerObject.m_savedEulerRotation.x, 0.f, 0.f));
			playerTransform.SetLocalRotation(glm::vec3(0.f, playerObject.m_savedEulerRotation.y, 0.f));

			return;
		}

		// Map mouse pixel deltas to pitch/yaw rotations in radians. This ensures that we have consistent mouse 
		// movement regardless of the resolution/aspect ratio/etc
		const float mousePxDeltaX = en::InputManager::GetRelativeMouseInput(en::Input_MouseX);
		const float mousePxDeltaY = en::InputManager::GetRelativeMouseInput(en::Input_MouseY);

		const float xRes = static_cast<float>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey));
		const float yRes = static_cast<float>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey));

		const float yFOV = camera.GetFieldOfViewYRad();
		const float xFOV = (xRes * yFOV) / yRes;

		constexpr float twoPi = 2.0f * static_cast<float>(std::numbers::pi);

		const float fullRotationResolutionY = (yRes * twoPi) / yFOV; // No. of pixels in a 360 degree (2pi) arc about X
		const float yRotationRadians = (mousePxDeltaY / fullRotationResolutionY) * twoPi;

		const float fullRotationResolutionX = (xRes * twoPi) / xFOV; // No. of pixels in a 360 degree (2pi) arc about Y
		const float xRotationRadians = (mousePxDeltaX / fullRotationResolutionX) * twoPi;

		// Apply the first person view orientation: (pitch + yaw)
		const glm::vec3 yaw(0.0f, xRotationRadians, 0.0f);
		const glm::vec3 pitch(yRotationRadians, 0.0f, 0.0f);
		playerTransform.RotateLocal(yaw);
		cameraTransform.RotateLocal(pitch);

		// Handle direction:
		glm::vec3 direction = glm::vec3(0.0f, 0.0f, 0.0f);

		if (en::InputManager::GetKeyboardInputState(en::InputButton_Forward))
		{
			direction -= cameraTransform.GetGlobalForward();
		}
		if (en::InputManager::GetKeyboardInputState(en::InputButton_Backward))
		{
			direction += cameraTransform.GetGlobalForward();
		}
		if (en::InputManager::GetKeyboardInputState(en::InputButton_Left))
		{
			direction -= cameraTransform.GetGlobalRight();
		}
		if (en::InputManager::GetKeyboardInputState(en::InputButton_Right))
		{
			direction += cameraTransform.GetGlobalRight();
		}
		if (en::InputManager::GetKeyboardInputState(en::InputButton_Up))
		{
			direction += playerTransform.GetGlobalUp(); // PlayerCam is tilted; use the parent transform instead
		}
		if (en::InputManager::GetKeyboardInputState(en::InputButton_Down))
		{
			direction -= playerTransform.GetGlobalUp(); // PlayerCam is tilted; use the parent transform instead
		}

		if (glm::length(direction) > 0.f) // Check the length since opposite inputs can zero out the direction
		{
			direction = glm::normalize(direction);

			float sprintModifier = 1.0f;
			if (en::InputManager::GetKeyboardInputState(en::InputButton_Sprint))
			{
				sprintModifier = playerObject.m_sprintSpeedModifier;
			}

			// Note: Velocity = (delta displacement) / (delta time)
			// delta displacement = velocity * delta time
			direction *= playerObject.m_movementSpeed * sprintModifier * static_cast<float>(stepTimeMs);

			playerTransform.TranslateLocal(direction);
		}


		// Save the current position/rotation:
		if (en::InputManager::GetMouseInputState(en::InputMouse_Right))
		{
			playerObject.m_savedPosition = playerTransform.GetGlobalPosition();
			playerObject.m_savedEulerRotation = glm::vec3(
				cameraTransform.GetLocalEulerXYZRotationRadians().x,
				playerTransform.GetGlobalEulerXYZRotationRadians().y,
				0);
		}
	}


	CameraControlComponent::CameraControlComponent()
		: m_movementSpeed(0.006f)
		, m_savedPosition(glm::vec3(0.0f, 0.0f, 0.0f))
		, m_savedEulerRotation(glm::vec3(0.0f, 0.0f, 0.0f))
	{
		m_sprintSpeedModifier = en::Config::Get()->GetValue<float>("sprintSpeedModifier");
	}
}
