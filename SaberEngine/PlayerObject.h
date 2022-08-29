#pragma once

#include "GameObject.h"	// Base class

#include <glm/glm.hpp>

using glm::vec3;


namespace gr
{
	class Camera;
}

namespace SaberEngine
{
	class PlayerObject : public virtual GameObject
	{
	public:
		PlayerObject(std::shared_ptr<gr::Camera> playerCam);

		~PlayerObject() = default;

		// Getters/Setters:
		inline std::shared_ptr<gr::Camera> GetCamera() { return m_playerCam; }

		// SaberObject interface:
		void Update() override;

		//// EventListener interface:
		//void HandleEvent(std::shared_ptr<EventInfo const> eventInfo) override;

	protected:


	private:
		std::shared_ptr<gr::Camera> m_playerCam;

		// Control configuration:
		float m_movementSpeed = 0.003f;

		// Saved positions
		vec3 m_savedPosition		= vec3(0.0f, 0.0f, 0.0f);
		vec3 m_savedEulerRotation = vec3(0.0f, 0.0f, 0.0f);
	};

}

