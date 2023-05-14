// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "EventListener.h"
#include "NamedObject.h"
#include "Transformable.h"
#include "Updateable.h"


namespace gr
{
	class Camera;
}

namespace fr
{
	class PlayerObject final
		: public virtual en::NamedObject
		, public virtual en::Updateable
		, public virtual fr::Transformable
		, public virtual en::EventListener
	{
	public:
		explicit PlayerObject(std::shared_ptr<gr::Camera> playerCam);
		PlayerObject(PlayerObject&&) = default;
		PlayerObject& operator=(PlayerObject&&) = default;
		~PlayerObject() = default;

		// NamedObject interface:
		void Update(const double stepTimeMs) override;

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


	private:
		PlayerObject() = delete;
		PlayerObject(PlayerObject const&) = delete;
		PlayerObject& operator=(PlayerObject const&) = delete;
	};

}

