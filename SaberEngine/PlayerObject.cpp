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

		// Move the yaw (ie. about Y) rotation from the camera to the PlayerObject's transform:

		Transform* playerCamTransform = m_playerCam->GetTransform();
		vec3 camRotation = playerCamTransform->GetEulerRotation();
		vec3 camPosition = playerCamTransform->GetWorldPosition();

		playerCamTransform->SetWorldRotation(vec3(camRotation.x, 0.0f, 0.0f));	// Set pitch
		playerCamTransform->SetWorldPosition(vec3(0.0f, 0.0f, 0.0f));			// Relative to PlayerObject parent
		
		m_transform.SetWorldRotation(vec3(0.0f, camRotation.y, 0.0f));		// Set yaw
		m_transform.SetWorldPosition(camPosition);
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

		m_transform.Rotate(yaw);
		m_playerCam->GetTransform()->Rotate(pitch);

		// Handle direction:
		vec3 direction = vec3(0.0f, 0.0f, 0.0f);

		if (InputManager::GetKeyboardInputState(en::InputButton_Forward))
		{
			vec3 forward = m_transform.Forward();
			Transform::RotateVector(forward, m_playerCam->GetTransform()->GetEulerRotation().x, m_transform.Right());

			direction -= forward;
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Backward))
		{
			vec3 forward = m_transform.Forward();
			Transform::RotateVector(forward, m_playerCam->GetTransform()->GetEulerRotation().x, m_transform.Right());

			direction += forward;
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Left))
		{
			direction -= m_transform.Right();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Right))
		{
			direction += m_transform.Right();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Up))
		{
			direction += m_transform.Up();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Down))
		{
			direction -= m_transform.Up();
		}

		if (glm::length(direction) != 0.0f)
		{
			direction = glm::normalize(direction);
			direction *= (float)(m_movementSpeed * TimeManager::DeltaTime());

			m_transform.Translate(direction);
		}

		// Reset the cam back to the saved position
		if (InputManager::GetMouseInputState(en::InputMouse_Left))
		{
			m_transform.SetWorldPosition(m_savedPosition);
			m_transform.SetWorldRotation(vec3(0, m_savedEulerRotation.y, 0));
			m_playerCam->GetTransform()->SetWorldRotation(vec3(m_savedEulerRotation.x, 0, 0));
		}

		// Save the current position/rotation:
		if (InputManager::GetMouseInputState(en::InputMouse_Right))
		{
			m_savedPosition = m_transform.GetWorldPosition();
			m_savedEulerRotation = vec3(m_playerCam->GetTransform()->GetEulerRotation().x, m_transform.GetEulerRotation().y, 0 );
		}
	}
}
