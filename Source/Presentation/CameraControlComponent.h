// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CameraComponent.h"


namespace fr
{
	class Camera;
	class EntityManager;


	class CameraControlComponent
	{
	public:
		struct PlayerObjectMarker {};

	public:
		static entt::entity CreateCameraControlConcept(EntityManager&, entt::entity cameraConcept);

		static void SetCamera(
			entt::entity camControlCmptOwner,
			entt::entity currentCamCmptOwner,
			entt::entity newCamCmptOwner);

		static void Update(
			CameraControlComponent&, 
			fr::Transform& playerTransform, 
			fr::Camera const&, 
			fr::Transform& cameraTransform, 
			double stepTimeMs);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity camControlEntity, entt::entity currentCam);


	public:
		CameraControlComponent();


	public:
		// Control configuration:
		float m_movementSpeed;
		float m_sprintSpeedModifier;

		// Sensitivity params:
		float m_mousePitchSensitivity;
		float m_mouseYawSensitivity;

		entt::entity m_prevCameraParentEntity;
		fr::Transform* m_prevCameraTransformParent;

		glm::vec3 m_prevLocalTranslation;
		glm::quat m_prevLocalRotation;
		glm::vec3 m_prevLocalScale;
	};
}

