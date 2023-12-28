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
		static entt::entity CreatePlayerObjectConcept(EntityManager&, entt::entity cameraConcept);

		static void SetCamera(EntityManager&, entt::entity playerEntity, entt::entity cameraConcept);

		static void Update(
			CameraControlComponent&, 
			fr::Transform& playerTransform, 
			fr::Camera const&, 
			fr::Transform& cameraTransform, 
			double stepTimeMs);


	public:
		CameraControlComponent();


	public:
		// Control configuration:
		float m_movementSpeed;
		float m_sprintSpeedModifier;

		// Saved location:
		glm::vec3 m_savedPosition;
		glm::vec3 m_savedEulerRotation;
	};
}

