// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/CameraRenderData.h"

#include "Renderer/Shaders/Common/CameraParams.h"


namespace pr
{
	class Transform;


	class Camera final
	{
	public:
		static CameraData BuildCameraData(pr::Camera const&);


	public:
		Camera(gr::Camera::Config const& camConfig, pr::Transform const* transform);

		Camera(Camera const&) = default;
		Camera(Camera&&) noexcept = default;
		Camera& operator=(Camera const&) = default;
		Camera& operator=(Camera&&) noexcept = default;
		~Camera() = default;

		float const GetFieldOfViewYRad() const;

		glm::vec2 GetNearFar() const;
		void SetNearFar(glm::vec2 const& nearFar);

		float GetAspectRatio() const;

		float GetAperture() const;
		void SetAperture(float aperture);

		float GetShutterSpeed() const;
		void SetShutterSpeed(float shutterSpeed);

		float GetSensitivity() const;
		void SetSensitivity(float sensitivity);

		gr::Camera::Config const& GetCameraConfig() const;
		void SetCameraConfig(gr::Camera::Config const& newConfig);

		pr::Transform const* GetTransform() const;

		bool SetActive(bool isActive);
		bool IsActive() const;

		bool IsDirty() const;
		void MarkClean();

		void ShowImGuiWindow(uint64_t uniqueID);


	private:
		pr::Transform const* m_transform; // We cache this for convenience due to a Camera's dependence on its Transform

		gr::Camera::Config m_cameraConfig;
		
		bool m_isActive; // Is this camera actively used to render things? If not, we'll skip culling for it

		bool m_isDirty;


	private: 
		Camera() = delete;
	};


	inline float const Camera::GetFieldOfViewYRad() const
	{
		return m_cameraConfig.m_yFOV;
	}


	inline glm::vec2 Camera::GetNearFar() const
	{
		return glm::vec2(m_cameraConfig.m_near, m_cameraConfig.m_far);
	}


	inline void Camera::SetNearFar(glm::vec2 const& nearFar)
	{
		m_cameraConfig.m_near = nearFar.x;
		m_cameraConfig.m_far = nearFar.y;
		m_isDirty = true;
	}


	inline float Camera::GetAspectRatio() const
	{
		return m_cameraConfig.m_aspectRatio;
	}


	inline float Camera::GetAperture() const
	{
		return m_cameraConfig.m_aperture;
	}


	inline void Camera::SetAperture(float aperture)
	{
		m_cameraConfig.m_aperture = aperture;
		m_isDirty = true;
	}

	inline float Camera::GetShutterSpeed() const
	{
		return m_cameraConfig.m_shutterSpeed;
	}


	inline void Camera::SetShutterSpeed(float shutterSpeed)
	{
		m_cameraConfig.m_shutterSpeed = shutterSpeed;
		m_isDirty = true;
	}


	inline float Camera::GetSensitivity() const
	{
		return m_cameraConfig.m_sensitivity;
	}


	inline void Camera::SetSensitivity(float sensitivity)
	{
		m_cameraConfig.m_sensitivity = sensitivity;
		m_isDirty = true;
	}


	inline gr::Camera::Config const& Camera::GetCameraConfig() const
	{
		return m_cameraConfig;
	}


	inline pr::Transform const* Camera::GetTransform() const
	{
		return m_transform;
	}


	inline bool Camera::SetActive(bool isActive)
	{
		if (m_isActive != isActive)
		{
			m_isDirty = true;
		}
		m_isActive = isActive;
		return m_isActive;
	}


	inline bool Camera::IsActive() const
	{
		return m_isActive;
	}


	inline bool Camera::IsDirty() const
	{
		return m_isDirty;
	}


	inline void Camera::MarkClean()
	{
		m_isDirty = false;
	}
}
