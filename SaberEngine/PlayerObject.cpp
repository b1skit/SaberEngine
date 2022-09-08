#include "PlayerObject.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "CoreEngine.h"
#include "Camera.h"

#include <glm/gtc/constants.hpp>

using gr::Transform;
using gr::Camera;
using glm::vec3;
using SaberEngine::InputManager;
using SaberEngine::TimeManager;


namespace fr
{
	PlayerObject::PlayerObject(std::shared_ptr<Camera> playerCam) : 
		SaberEngine::GameObject::GameObject("Player Object"), SceneObject("Player Object"),
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
		yaw.y	= (float)InputManager::GetMouseAxisInput(SaberEngine::INPUT_MOUSE_X) * (float)TimeManager::DeltaTime();
		pitch.x = (float)InputManager::GetMouseAxisInput(SaberEngine::INPUT_MOUSE_Y) * (float)TimeManager::DeltaTime();

		m_transform.Rotate(yaw);
		m_playerCam->GetTransform()->Rotate(pitch);

		// Handle direction:
		vec3 direction = vec3(0.0f, 0.0f, 0.0f);

		if (InputManager::GetKeyboardInputState(SaberEngine::INPUT_BUTTON_FORWARD))
		{
			vec3 forward = m_transform.Forward();
			Transform::RotateVector(forward, m_playerCam->GetTransform()->GetEulerRotation().x, m_transform.Right());

			direction -= forward;
		}
		if (InputManager::GetKeyboardInputState(SaberEngine::INPUT_BUTTON_BACKWARD))
		{
			vec3 forward = m_transform.Forward();
			Transform::RotateVector(forward, m_playerCam->GetTransform()->GetEulerRotation().x, m_transform.Right());

			direction += forward;
		}
		if (InputManager::GetKeyboardInputState(SaberEngine::INPUT_BUTTON_LEFT))
		{
			direction -= m_transform.Right();
		}
		if (InputManager::GetKeyboardInputState(SaberEngine::INPUT_BUTTON_RIGHT))
		{
			direction += m_transform.Right();
		}
		if (InputManager::GetKeyboardInputState(SaberEngine::INPUT_BUTTON_UP))
		{
			direction += m_transform.Up();
		}
		if (InputManager::GetKeyboardInputState(SaberEngine::INPUT_BUTTON_DOWN))
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
		if (InputManager::GetMouseInputState(SaberEngine::INPUT_MOUSE_LEFT))
		{
			m_transform.SetWorldPosition(m_savedPosition);
			m_transform.SetWorldRotation(vec3(0, m_savedEulerRotation.y, 0));
			m_playerCam->GetTransform()->SetWorldRotation(vec3(m_savedEulerRotation.x, 0, 0));
		}

		// Save the current position/rotation:
		if (InputManager::GetMouseInputState(SaberEngine::INPUT_MOUSE_RIGHT))
		{
			m_savedPosition = m_transform.GetWorldPosition();
			m_savedEulerRotation = vec3(m_playerCam->GetTransform()->GetEulerRotation().x, m_transform.GetEulerRotation().y, 0 );
		}
	}
}
