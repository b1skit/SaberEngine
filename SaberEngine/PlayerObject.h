#pragma once

#include "GameObject.h"	// Base class

#include <glm/glm.hpp>

using glm::vec3;


namespace SaberEngine
{
	// Pre-declarations:
	class Camera;

	class PlayerObject : public GameObject
	{
	public:
		PlayerObject(Camera* playerCam);


		// Getters/Setters:
		inline Camera* GetCamera() { return m_playerCam; }

		// SaberObject interface:
		void Update() override;

		//// EventListener interface:
		//void HandleEvent(EventInfo const* eventInfo) override;

	protected:


	private:
		Camera* m_playerCam;

		// Control configuration:
		float m_movementSpeed = 0.003f;

		// Saved positions
		vec3 m_savedPosition		= vec3(0.0f, 0.0f, 0.0f);
		vec3 m_savedEulerRotation = vec3(0.0f, 0.0f, 0.0f);
	};

}

