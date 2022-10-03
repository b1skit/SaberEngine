#include "PlayerObject.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "CoreEngine.h"
#include "Camera.h"

#include <glm/gtc/constants.hpp>

using gr::Transform;
using gr::Camera;
using glm::vec3;
using en::InputManager;
using en::TimeManager;


namespace fr
{
	PlayerObject::PlayerObject(std::shared_ptr<Camera> playerCam) : 
		fr::GameObject("Player Object"), SceneObject("Player Object"),
		m_playerCam(playerCam),
		m_movementSpeed(0.003f),
		m_savedPosition(vec3(0.0f, 0.0f, 0.0f)),
		m_savedEulerRotation(vec3(0.0f, 0.0f, 0.0f))
	{
		m_playerCam->GetTransform()->SetParent(&m_transform);
		m_sprintSpeedModifier = en::CoreEngine::GetConfig()->GetValue<float>("sprintSpeedModifier");
	}


	void PlayerObject::Update()
	{
		GameObject::Update();

		// Handle first person view orientation: (pitch + yaw)
		vec3 yaw(0.0f, 0.0f, 0.0f);
		vec3 pitch(0.0f, 0.0f, 0.0f);

		// Compute rotation amounts, in radians:
		yaw.y	= (float)InputManager::GetMouseAxisInput(en::Input_MouseX) * (float)TimeManager::DeltaTime();
		pitch.x = (float)InputManager::GetMouseAxisInput(en::Input_MouseY) * (float)TimeManager::DeltaTime();

		m_transform.RotateModel(yaw);
		m_playerCam->GetTransform()->RotateModel(pitch);

		// Handle direction:
		vec3 direction = vec3(0.0f, 0.0f, 0.0f);

		if (InputManager::GetKeyboardInputState(en::InputButton_Forward))
		{		
			direction -= m_playerCam->GetTransform()->ForwardWorld();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Backward))
		{
			direction += m_playerCam->GetTransform()->ForwardWorld();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Left))
		{
			direction -= m_playerCam->GetTransform()->RightWorld();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Right))
		{
			direction += m_playerCam->GetTransform()->RightWorld();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Up))
		{
			direction += m_transform.UpWorld(); // PlayerCam is tilted; use the parent transform instead
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Down))
		{
			direction -= m_transform.UpWorld(); // PlayerCam is tilted; use the parent transform instead
		}

		float sprintModifier = 1.0f;
		if (InputManager::GetKeyboardInputState(en::InputButton_Sprint))
		{
			sprintModifier = m_sprintSpeedModifier;
		}

		if (glm::length(direction) != 0.0f)
		{
			direction = glm::normalize(direction);
			direction *= (float)(m_movementSpeed * sprintModifier * TimeManager::DeltaTime());

			m_transform.TranslateModel(direction);
		}

		// Reset the cam back to the saved position
		if (InputManager::GetMouseInputState(en::InputMouse_Left))
		{
			m_transform.SetModelPosition(m_savedPosition);
			m_transform.SetModelRotation(vec3(0, m_savedEulerRotation.y, 0));
			m_playerCam->GetTransform()->SetModelRotation(vec3(m_savedEulerRotation.x, 0, 0));
		}

		// Save the current position/rotation:
		if (InputManager::GetMouseInputState(en::InputMouse_Right))
		{
			m_savedPosition = m_transform.GetWorldPosition();
			m_savedEulerRotation = vec3(
				m_playerCam->GetTransform()->GetModelEulerXYZRotationRadians().x, 
				m_transform.GetWorldEulerXYZRotationRadians().y, 
				0 );
		}
	}
}
