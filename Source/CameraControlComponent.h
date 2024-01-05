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
			fr::TransformComponent& controllerTransformCmpt,
			fr::TransformComponent* currentCamTransformCmpt, 
			fr::TransformComponent& newCamTransformCmpt);

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

		// Saved location:
		glm::vec3 m_savedPosition;
		glm::vec3 m_savedEulerRotation;
	};
}

