#pragma once

#include "NamedObject.h"
#include "Updateable.h"
#include "Transformable.h"
#include "EventListener.h"
#include <glm/glm.hpp>


namespace gr
{
	class Camera;
}

namespace fr
{
	class PlayerObject 
		: public virtual en::NamedObject
		, public virtual en::Updateable
		, public virtual fr::Transformable
		, public virtual en::EventListener
	{
	public:
		explicit PlayerObject(std::shared_ptr<gr::Camera> playerCam);
		~PlayerObject() = default;

		PlayerObject() = delete;
		PlayerObject(PlayerObject const&) = delete;
		PlayerObject(PlayerObject&&) = delete;
		PlayerObject& operator=(PlayerObject const&) = delete;

		// NamedObject interface:
		void Update() override;

		// EventListener:
		void HandleEvents() override;

		// Getters/Setters:
		inline std::shared_ptr<gr::Camera> GetCamera() { return m_playerCam; }


	private:
		std::shared_ptr<gr::Camera> m_playerCam;

		bool m_processInput;

		// Control configuration:
		float m_movementSpeed;
		float m_sprintSpeedModifier;

		// Saved location:
		glm::vec3 m_savedPosition;
		glm::vec3 m_savedEulerRotation;
	};

}

