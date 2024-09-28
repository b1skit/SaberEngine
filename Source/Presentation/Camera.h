// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/CameraRenderData.h"

#include "Shaders/Common/CameraParams.h"


namespace fr
{
	class Transform;


	class Camera
	{
	public:
		static CameraData BuildCameraData(fr::Camera const&);


	public:
		Camera(gr::Camera::Config const& camConfig, fr::Transform const* transform);

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

		fr::Transform const* GetTransform() const;

		bool IsDirty() const;
		void MarkClean();

		void ShowImGuiWindow(uint64_t uniqueID);


	private:
		fr::Transform const* m_transform; // We cache this for convenience due to a Camera's dependence on its Transform

		gr::Camera::Config m_cameraConfig;
		
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


	inline fr::Transform const* Camera::GetTransform() const
	{
		return m_transform;
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
