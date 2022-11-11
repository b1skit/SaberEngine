#include "PlayerObject.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "Config.h"
#include "Camera.h"

#include <glm/gtc/constants.hpp>

using gr::Transform;
using gr::Camera;
using en::Config;
using en::InputManager;
using en::TimeManager;
using glm::vec3;


namespace fr
{
	PlayerObject::PlayerObject(std::shared_ptr<Camera> playerCam) : en::NamedObject("Player Object"),
		m_playerCam(playerCam),
		m_movementSpeed(0.003f),
		m_savedPosition(vec3(0.0f, 0.0f, 0.0f)),
		m_savedEulerRotation(vec3(0.0f, 0.0f, 0.0f))
	{
		// The PlayerObject and Camera must be located at the same point. To avoid stomping imported Camera locations,
		// we move the PlayerObject to the camera. Then, we re-parent the Camera's Transform, to maintain its global
		// orientation but update its local orientation under the PlayerObject Transform
		m_transform.SetGlobalTranslation(m_playerCam->GetTransform()->GetGlobalPosition());
		m_playerCam->GetTransform()->ReParent(&m_transform);

		m_sprintSpeedModifier = Config::Get()->GetValue<float>("sprintSpeedModifier");
	}


	void PlayerObject::Update()
	{
		// Handle first person view orientation: (pitch + yaw)
		vec3 yaw(0.0f, 0.0f, 0.0f);
		vec3 pitch(0.0f, 0.0f, 0.0f);

		// Compute rotation amounts, in radians:
		yaw.y	= (float)InputManager::GetMouseAxisInput(en::Input_MouseX) * (float)TimeManager::DeltaTime();
		pitch.x = (float)InputManager::GetMouseAxisInput(en::Input_MouseY) * (float)TimeManager::DeltaTime();

		m_transform.RotateLocal(yaw);
		m_playerCam->GetTransform()->RotateLocal(pitch);

		// Handle direction:
		vec3 direction = vec3(0.0f, 0.0f, 0.0f);

		if (InputManager::GetKeyboardInputState(en::InputButton_Forward))
		{		
			direction -= m_playerCam->GetTransform()->GetGlobalForward();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Backward))
		{
			direction += m_playerCam->GetTransform()->GetGlobalForward();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Left))
		{
			direction -= m_playerCam->GetTransform()->GetGlobalRight();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Right))
		{
			direction += m_playerCam->GetTransform()->GetGlobalRight();
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Up))
		{
			direction += m_transform.GetGlobalUp(); // PlayerCam is tilted; use the parent transform instead
		}
		if (InputManager::GetKeyboardInputState(en::InputButton_Down))
		{
			direction -= m_transform.GetGlobalUp(); // PlayerCam is tilted; use the parent transform instead
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

			m_transform.TranslateLocal(direction);
		}

		// Reset the cam back to the saved position
		if (InputManager::GetMouseInputState(en::InputMouse_Left))
		{
			m_transform.SetLocalTranslation(m_savedPosition);
			m_transform.SetLocalRotation(vec3(0, m_savedEulerRotation.y, 0));
			m_playerCam->GetTransform()->SetLocalRotation(vec3(m_savedEulerRotation.x, 0, 0));
		}

		// Save the current position/rotation:
		if (InputManager::GetMouseInputState(en::InputMouse_Right))
		{
			m_savedPosition = m_transform.GetGlobalPosition();
			m_savedEulerRotation = vec3(
				m_playerCam->GetTransform()->GetLocalEulerXYZRotationRadians().x, 
				m_transform.GetGlobalEulerXYZRotationRadians().y, 
				0 );
		}
	}
}
