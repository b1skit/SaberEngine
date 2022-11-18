#include "PlayerObject.h"
#include "InputManager.h"
#include <glm/gtc/constants.hpp>

#include "Config.h"
#include "Camera.h"
#include "EventManager.h"

using gr::Transform;
using gr::Camera;
using en::Config;
using en::InputManager;
using glm::vec3;


namespace fr
{
	PlayerObject::PlayerObject(std::shared_ptr<Camera> playerCam)
		: en::NamedObject("Player Object")
		, m_playerCam(playerCam)
		, m_processInput(true)
		, m_movementSpeed(0.006f)
		, m_savedPosition(vec3(0.0f, 0.0f, 0.0f))
		, m_savedEulerRotation(vec3(0.0f, 0.0f, 0.0f))
	{
		// The PlayerObject and Camera must be located at the same point. To avoid stomping imported Camera locations,
		// we move the PlayerObject to the camera. Then, we re-parent the Camera's Transform, to maintain its global
		// orientation but update its local orientation under the PlayerObject Transform
		m_transform.SetGlobalTranslation(m_playerCam->GetTransform()->GetGlobalPosition());
		m_playerCam->GetTransform()->ReParent(&m_transform);

		m_sprintSpeedModifier = Config::Get()->GetValue<float>("sprintSpeedModifier");

		// Subscribe to events:
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::InputToggleConsole, this);
	}


	void PlayerObject::HandleEvents()
	{
		while (HasEvents())
		{
			en::EventManager::EventInfo eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case en::EventManager::EventType::InputToggleConsole:
			{
				// Only enable/disable input processing when the console button is toggled
				if (eventInfo.m_data0.m_dataB)
				{
					m_processInput = !m_processInput;
				}
			}
			break;
			default:
				break;
			}
		}
	}


	void PlayerObject::Update(const double stepTimeMs)
	{
		HandleEvents();

		if (!m_processInput)
		{
			return;
		}

		// Reset the cam back to the saved position
		if (InputManager::GetMouseInputState(en::InputMouse_Left))
		{
			m_transform.SetLocalTranslation(m_savedPosition);
			m_playerCam->GetTransform()->SetLocalRotation(vec3(m_savedEulerRotation.x, 0, 0));
			m_transform.SetLocalRotation(vec3(0, m_savedEulerRotation.y, 0));

			return;
		}

		// Handle first person view orientation: (pitch + yaw)
		vec3 yaw(0.0f, 0.0f, 0.0f);
		vec3 pitch(0.0f, 0.0f, 0.0f);

		// Compute camera pitch/yaw rotations using the raw mouse pixel deltas.
		// TODO: We might get better/more consistent results if we map our mouse pixel deltas to 2pi radians, with
		// respect to the resolution, field of view, and aspect ratio.
		yaw.y = InputManager::GetMouseAxisInput(en::Input_MouseX);
		pitch.x = InputManager::GetMouseAxisInput(en::Input_MouseY);

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

		if (glm::length(direction) > 0.f) // Check the length since opposite inputs can zero out the direction
		{
			direction = glm::normalize(direction);

			float sprintModifier = 1.0f;
			if (InputManager::GetKeyboardInputState(en::InputButton_Sprint))
			{
				sprintModifier = m_sprintSpeedModifier;
			}

			// Speed is distance/ms
			direction *= m_movementSpeed * sprintModifier / static_cast<float>(stepTimeMs);

			m_transform.TranslateLocal(direction);
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
