#include <numbers>

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
		, fr::Transformable(nullptr)
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

		// Map mouse pixel deltas to pitch/yaw rotations in radians. This ensures that we have consistent mouse 
		// movement regardless of the resolution/aspect ratio/etc
		const float mousePxDeltaX = InputManager::GetMouseAxisInput(en::Input_MouseX); // RENAME
		const float mousePxDeltaY = InputManager::GetMouseAxisInput(en::Input_MouseY); // RENAME yPixelDelta
		
		const float xRes = static_cast<float>(Config::Get()->GetValue<int>("windowXRes"));
		const float yRes = static_cast<float>(Config::Get()->GetValue<int>("windowYRes"));
		
		const float yFOV = m_playerCam->FieldOfViewYRad();
		const float xFOV = (xRes * yFOV) / yRes;

		constexpr float twoPi = 2.0f * static_cast<float>(std::numbers::pi);

		const float fullRotationResolutionY = (yRes * twoPi) / yFOV; // No. of pixels in a 360 degree (2pi) arc about X
		const float yRotationRadians = (mousePxDeltaY / fullRotationResolutionY) * twoPi;

		const float fullRotationResolutionX = (xRes * twoPi) / xFOV; // No. of pixels in a 360 degree (2pi) arc about Y
		const float xRotationRadians = (mousePxDeltaX / fullRotationResolutionX) * twoPi;

		// Apply the first person view orientation: (pitch + yaw)
		const vec3 yaw(0.0f, xRotationRadians, 0.0f);
		const vec3 pitch(yRotationRadians, 0.0f, 0.0f);
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

			// Note: Velocity = (delta displacement) / (delta time)
			// delta displacement = velocity * delta time
			direction *= m_movementSpeed * sprintModifier * static_cast<float>(stepTimeMs);

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
