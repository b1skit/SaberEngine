// © 2022 Adam Badke. All rights reserved.
#include "Private/AnimationComponent.h"
#include "Private/CameraComponent.h"
#include "Private/CameraControlComponent.h"
#include "Private/EntityManager.h"
#include "Private/NameComponent.h"
#include "Private/RelationshipComponent.h"
#include "Private/SceneNodeConcept.h"
#include "Private/TransformComponent.h"

#include "Core/Config.h"
#include "Core/InputManager.h"


namespace
{
	constexpr char const* k_defaultCameraControllerName = "FPS Camera Controller";
}

namespace fr
{
	entt::entity CameraControlComponent::CreateCameraControlConcept(EntityManager& em, entt::entity camEntity)
	{
		SEAssert(camEntity == entt::null ||
			em.HasComponent<fr::CameraComponent>(camEntity),
			"camEntity must have a CameraComponent attached");

		entt::entity camControlNode = fr::SceneNode::Create(em, k_defaultCameraControllerName, entt::null);

		em.EmplaceComponent<CameraControlComponent>(camControlNode);

		fr::TransformComponent::AttachTransformComponent(em, camControlNode);

		// Attach the camera to the camera controller:
		if (camEntity != entt::null)
		{
			SetCamera(camControlNode, entt::null, camEntity);
		}

		return camControlNode;
	}


	void CameraControlComponent::SetCamera(
		entt::entity camControlCmptEntity,
		entt::entity currentCamCmptEntity,
		entt::entity newCamCmptEntity)
	{	
		fr::EntityManager& em = *fr::EntityManager::Get();

		// The CameraControlComponent gimbal requires 2 Transforms (for pitch/yaw), animations target a single Transform
		SEAssert(!em.HasComponent<fr::AnimationComponent>(newCamCmptEntity),
			"The target camera has an AnimationComponent, it cannot be controlled by a camera controller as well");

		SEAssert(em.HasComponent<fr::TransformComponent>(camControlCmptEntity),
			"CameraControlComponent owning entity must have a TransformComponent");

		fr::CameraControlComponent& camControlCmpt = em.GetComponent<fr::CameraControlComponent>(camControlCmptEntity);

		// Reparent the existing camera (if any) to a null parent. This effectively collapses the global transform
		// values to the local transform, so the camera's final location remains the same
		if (currentCamCmptEntity != entt::null)
		{
			SEAssert(em.HasComponent<fr::TransformComponent>(currentCamCmptEntity),
				"Owning entity for the current camera component does not have a TransformComponent. This is unexpected");

			fr::TransformComponent& currentCamTransformCmpt = em.GetComponent<fr::TransformComponent>(currentCamCmptEntity);

			// Restore the previous hierarchy to the camera:
			fr::Transform& currentCamTransform = currentCamTransformCmpt.GetTransform();
			currentCamTransform.ReParent(camControlCmpt.m_prevCameraTransformParent);

			currentCamTransform.SetLocalTranslation(camControlCmpt.m_prevLocalTranslation);
			currentCamTransform.SetLocalRotation(camControlCmpt.m_prevLocalRotation);
			currentCamTransform.SetLocalScale(camControlCmpt.m_prevLocalScale);

			fr::Relationship& currentCamRelationship = em.GetComponent<fr::Relationship>(currentCamCmptEntity);
			currentCamRelationship.SetParent(em, camControlCmpt.m_prevCameraParentEntity);

			// Clear the cached hierarchy records:
			camControlCmpt.m_prevCameraParentEntity = entt::null;
			camControlCmpt.m_prevCameraTransformParent = nullptr;
		}
		
		// Attach the new camera (if any) to the controller:
		if (newCamCmptEntity != entt::null)
		{
			SEAssert(em.HasComponent<fr::TransformComponent>(newCamCmptEntity),
				"Owning entity for the new camera component does not have a TransformComponent. This is unexpected");

			fr::Transform& controllerTransform =
				em.GetComponent<fr::TransformComponent>(camControlCmptEntity).GetTransform();

			fr::Transform& newCamTransform = em.GetComponent<fr::TransformComponent>(newCamCmptEntity).GetTransform();
			camControlCmpt.m_prevCameraTransformParent = newCamTransform.GetParent();

			// Store the previous local Translation so we can restore it later. We need to Recompute() to ensure the Transform is not dirty
			newCamTransform.Recompute();
			camControlCmpt.m_prevLocalTranslation = newCamTransform.GetLocalTranslation();
			camControlCmpt.m_prevLocalRotation = newCamTransform.GetLocalRotation();
			camControlCmpt.m_prevLocalScale = newCamTransform.GetLocalScale();

			// The controller and Camera must be located at the same point. To avoid stomping imported Camera locations,
			// we move the camera controller to the camera. Then, we re-parent the Camera's Transform, to maintain its
			// global orientation but update its local orientation under the camera controller's Transform
			controllerTransform.SetGlobalTranslation(newCamTransform.GetGlobalTranslation());
			newCamTransform.ReParent(&controllerTransform);

			fr::Relationship& currentCamRelationship = em.GetComponent<fr::Relationship>(newCamCmptEntity);
			camControlCmpt.m_prevCameraParentEntity = currentCamRelationship.GetParent();
			currentCamRelationship.SetParent(em, camControlCmptEntity);
		}
	}


	void CameraControlComponent::Update(
		CameraControlComponent& camController, 
		fr::Transform& controllerTransform,
		fr::Camera const& camera,
		fr::Transform& cameraTransform,
		double stepTimeMs)
	{
		SEAssert(cameraTransform.GetParent() == &controllerTransform,
			"Camera transform must be parented to the camera controller's transform");

		// Map mouse pixel deltas to pitch/yaw rotations in radians. This ensures that we have consistent mouse 
		// movement regardless of the resolution/aspect ratio/etc
		const float mousePxDeltaX = 
			en::InputManager::GetRelativeMouseInput(definitions::Input_MouseX) * camController.m_mousePitchSensitivity * -1;
		const float mousePxDeltaY = 
			en::InputManager::GetRelativeMouseInput(definitions::Input_MouseY) * camController.m_mouseYawSensitivity * -1;

		const float xRes = static_cast<float>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey));
		const float yRes = static_cast<float>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey));

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
		if (glm::length(yaw) > 0.f)
		{
			controllerTransform.RotateLocal(yaw);
		}
		if (glm::length(pitch) > 0.f)
		{
			cameraTransform.RotateLocal(pitch);
		}

		// Handle direction:
		glm::vec3 direction = glm::vec3(0.0f, 0.0f, 0.0f);

		if (en::InputManager::GetKeyboardInputState(definitions::InputButton_Forward))
		{
			direction -= cameraTransform.GetGlobalForward();
		}
		if (en::InputManager::GetKeyboardInputState(definitions::InputButton_Backward))
		{
			direction += cameraTransform.GetGlobalForward();
		}
		if (en::InputManager::GetKeyboardInputState(definitions::InputButton_Left))
		{
			direction -= cameraTransform.GetGlobalRight();
		}
		if (en::InputManager::GetKeyboardInputState(definitions::InputButton_Right))
		{
			direction += cameraTransform.GetGlobalRight();
		}
		if (en::InputManager::GetKeyboardInputState(definitions::InputButton_Up))
		{
			direction += controllerTransform.GetGlobalUp(); // Cam is tilted; use the parent transform instead
		}
		if (en::InputManager::GetKeyboardInputState(definitions::InputButton_Down))
		{
			direction -= controllerTransform.GetGlobalUp(); // Cam is tilted; use the parent transform instead
		}

		if (glm::length(direction) > 0.f) // Check the length since opposite inputs can zero out the direction
		{
			direction = glm::normalize(direction);

			float sprintModifier = 1.0f;
			if (en::InputManager::GetKeyboardInputState(definitions::InputButton_Sprint))
			{
				sprintModifier = camController.m_sprintSpeedModifier;
			}

			// Note: Velocity = (delta displacement) / (delta time)
			// delta displacement = velocity * delta time
			direction *= camController.m_movementSpeed * sprintModifier * static_cast<float>(stepTimeMs);

			controllerTransform.TranslateLocal(direction);
		}
	}


	void CameraControlComponent::ShowImGuiWindow(
		fr::EntityManager& em, entt::entity camControlEntity, entt::entity currentCam)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(camControlEntity);

		if (ImGui::CollapsingHeader(
			std::format("Camera controller \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
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

			// Transform:
			fr::TransformComponent::ShowImGuiWindow(em, camControlEntity, nameCmpt.GetUniqueID());

			// Camera: Push/pop IDs to prevent a collision if the Camera menu is also expanded
			ImGui::PushID(std::format("Camera controller \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str());
			fr::CameraComponent::ShowImGuiWindow(em, currentCam);
			ImGui::PopID();

			ImGui::Unindent();
		}
	}

	CameraControlComponent::CameraControlComponent()
		: m_movementSpeed(0.006f)
		, m_prevCameraParentEntity(entt::null)
		, m_prevCameraTransformParent(nullptr)
		, m_prevLocalTranslation(0.f, 0.f, 0.f)
		, m_prevLocalRotation(1.f, 0.f, 0.f, 0.f)
		, m_prevLocalScale(1.f, 1.f, 1.f)
	{
		m_sprintSpeedModifier = core::Config::Get()->GetValue<float>(core::configkeys::k_sprintSpeedModifierKey);

		m_mousePitchSensitivity = core::Config::Get()->GetValue<float>(core::configkeys::k_mousePitchSensitivityKey);
		m_mouseYawSensitivity = core::Config::Get()->GetValue<float>(core::configkeys::k_mouseYawSensitivityKey);
	}
}
