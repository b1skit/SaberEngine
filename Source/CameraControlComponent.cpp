// © 2022 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "CameraControlComponent.h"
#include "Config.h"
#include "EntityManager.h"
#include "InputManager.h"
#include "NameComponent.h"
#include "TransformComponent.h"


namespace
{
	constexpr char const* k_defaultCameraControllerName = "FPS Camera Controller";
}

namespace fr
{
	entt::entity CameraControlComponent::CreateCameraControlConcept(EntityManager& em, entt::entity cameraConcept)
	{
		SEAssert("cameraConcept entity must have a CameraComponent attached",
			em.HasComponent<fr::CameraComponent>(cameraConcept));

		entt::entity camControlEntity = em.CreateEntity(k_defaultCameraControllerName);

		em.EmplaceComponent<CameraControlComponent>(camControlEntity);

		fr::TransformComponent& controllerTransform = 
			fr::TransformComponent::AttachTransformComponent(em, camControlEntity, nullptr);

		fr::Relationship const& cameraRelationship = em.GetComponent<fr::Relationship>(cameraConcept);
		fr::TransformComponent* camTransform =
			em.GetFirstInHierarchyAbove<fr::TransformComponent>(cameraRelationship.GetParent());
		SEAssert("Failed to find camera TransformComponent", camTransform);

		// Parent the camera to the player object:
		SetCamera(controllerTransform, *camTransform);
		
		return camControlEntity;
	}


	void CameraControlComponent::SetCamera(
		fr::TransformComponent& controllerTransformCmpt, fr::TransformComponent& camTransformCmpt)
	{
		fr::Transform& controllerTransform = controllerTransformCmpt.GetTransform();
		fr::Transform& cameraTransform = camTransformCmpt.GetTransform();

		// The PlayerObject and Camera must be located at the same point. To avoid stomping imported Camera locations,
		// we move the camera controller to the camera. Then, we re-parent the Camera's Transform, to maintain its
		// global orientation but update its local orientation under the camera controller's Transform
		controllerTransform.SetGlobalPosition(cameraTransform.GetGlobalPosition());
		cameraTransform.ReParent(&controllerTransform);

		// Note: We don't set a Relationship between the camera controller and a camera; A camera will already have been
		// parented to a SceneNode concept (and thus uses its transform).
	}


	void CameraControlComponent::Update(
		CameraControlComponent& camController, 
		fr::Transform& playerTransform,
		fr::Camera const& camera,
		fr::Transform& cameraTransform,
		double stepTimeMs)
	{
		SEAssert("Camera transform must be parented to the player transform", 
			cameraTransform.GetParent() == &playerTransform);

		// Reset the cam back to the saved position
		if (en::InputManager::GetMouseInputState(en::InputMouse_Left))
		{
			playerTransform.SetLocalPosition(camController.m_savedPosition);
			cameraTransform.SetLocalRotation(glm::vec3(camController.m_savedEulerRotation.x, 0.f, 0.f));
			playerTransform.SetLocalRotation(glm::vec3(0.f, camController.m_savedEulerRotation.y, 0.f));

			return;
		}

		// Map mouse pixel deltas to pitch/yaw rotations in radians. This ensures that we have consistent mouse 
		// movement regardless of the resolution/aspect ratio/etc
		const float mousePxDeltaX = 
			en::InputManager::GetRelativeMouseInput(en::Input_MouseX) * camController.m_mousePitchSensitivity * -1;
		const float mousePxDeltaY = 
			en::InputManager::GetRelativeMouseInput(en::Input_MouseY) * camController.m_mouseYawSensitivity * -1;

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
				sprintModifier = camController.m_sprintSpeedModifier;
			}

			// Note: Velocity = (delta displacement) / (delta time)
			// delta displacement = velocity * delta time
			direction *= camController.m_movementSpeed * sprintModifier * static_cast<float>(stepTimeMs);

			playerTransform.TranslateLocal(direction);
		}

		// Save the current position/rotation:
		if (en::InputManager::GetMouseInputState(en::InputMouse_Right))
		{
			camController.m_savedPosition = playerTransform.GetGlobalPosition();
			camController.m_savedEulerRotation = glm::vec3(
				cameraTransform.GetLocalEulerXYZRotationRadians().x,
				playerTransform.GetGlobalEulerXYZRotationRadians().y,
				0);
		}
	}


	void CameraControlComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity camControlEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(camControlEntity);

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			CameraControlComponent& camControlCmpt = em.GetComponent<CameraControlComponent>(camControlEntity);

			ImGui::SliderFloat(
				std::format("Movement speed##{}", nameCmpt.GetUniqueID()).c_str(),
				&camControlCmpt.m_movementSpeed,
				0.f,
				0.1f);

			ImGui::SliderFloat(
				std::format("Sprint speed modifier##{}", nameCmpt.GetUniqueID()).c_str(),
				&camControlCmpt.m_sprintSpeedModifier,
				0.f,
				5.f);

			ImGui::SliderFloat(
				std::format("Mouse pitch sensitivity##{}", nameCmpt.GetUniqueID()).c_str(),
				&camControlCmpt.m_mousePitchSensitivity,
				0.f,
				2.f);

			ImGui::SliderFloat(
				std::format("Mouse yaw sensitivity##{}", nameCmpt.GetUniqueID()).c_str(),
				&camControlCmpt.m_mouseYawSensitivity,
				0.f,
				2.f);

			ImVec2 buttonSize = ImVec2(-FLT_MIN, 0.0f);
			ImGui::BeginDisabled(true);
			if (ImGui::Button("Save settings"))
			{
				// TODO: Implement this
			}
			ImGui::EndDisabled();

			ImGui::Text(std::format("Saved position: {}", glm::to_string(camControlCmpt.m_savedPosition)).c_str());
			ImGui::Text(std::format("Saved Euler rotation: {}", glm::to_string(camControlCmpt.m_savedEulerRotation)).c_str());

			// Transform:
			fr::TransformComponent::ShowImGuiWindow(em, camControlEntity, nameCmpt.GetUniqueID());

			// Camera:
			// ECS_CONVERSION: TODO: Figure out how to link camera and camera controller...
			// -> Currently don't have a Relationship
			//		-> Perhaps Cameras should always have a Transform of their own?

			ImGui::Unindent();
		}
	}

	CameraControlComponent::CameraControlComponent()
		: m_movementSpeed(0.006f)
		, m_savedPosition(glm::vec3(0.0f, 0.0f, 0.0f))
		, m_savedEulerRotation(glm::vec3(0.0f, 0.0f, 0.0f))
	{
		m_sprintSpeedModifier = en::Config::Get()->GetValue<float>(en::ConfigKeys::k_sprintSpeedModifier);

		m_mousePitchSensitivity = en::Config::Get()->GetValue<float>(en::ConfigKeys::k_mousePitchSensitivity);
		m_mouseYawSensitivity = en::Config::Get()->GetValue<float>(en::ConfigKeys::k_mouseYawSensitivity);
	}
}
