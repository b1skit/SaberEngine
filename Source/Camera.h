// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"


namespace fr
{
	class Transform;


	class Camera
	{
	public:
		static gr::Camera::CameraParams BuildCameraParams(fr::Camera const&);


	public:
		Camera(gr::Camera::Config const& camConfig, fr::Transform const* transform);

		Camera(Camera const&) = default;
		Camera(Camera&&) = default;
		Camera& operator=(Camera const&) = default;
		Camera& operator=(Camera&&) = default;
		~Camera() = default;

		float const GetFieldOfViewYRad() const;

		glm::vec2 GetNearFar() const;
		void SetNearFar(glm::vec2 const& nearFar);

		float GetAspectRatio() const;

		glm::mat4 GetViewMatrix() const;
		glm::mat4 const& GetInverseViewMatrix() const;

		glm::mat4 const& GetProjectionMatrix() const;
		glm::mat4 GetInverseProjectionMatrix() const;

		glm::mat4 GetViewProjectionMatrix() const;
		glm::mat4 GetInverseViewProjectionMatrix() const;
	
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

		void ShowImGuiWindow();


	private:
		fr::Transform const* m_transform; // We cache this for convenience due to a Camera's dependence on its Transform

		gr::Camera::Config m_cameraConfig;

		glm::mat4 m_view;
		glm::mat4 m_invView;

		glm::mat4 m_projection;
		glm::mat4 m_invProjection;

		glm::mat4 m_viewProjection;
		glm::mat4 m_invViewProjection;
		
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


	inline glm::mat4 Camera::GetViewMatrix() const
	{
		return m_view;
	}


	inline glm::mat4 const& Camera::GetInverseViewMatrix() const
	{
		return m_invView;
	}


	inline glm::mat4 const& Camera::GetProjectionMatrix() const
	{
		return m_projection;
	}


	inline glm::mat4 Camera::GetInverseProjectionMatrix() const
	{
		return m_invProjection;
	}

	inline glm::mat4 Camera::GetViewProjectionMatrix() const
	{
		return m_viewProjection;
	}


	inline glm::mat4 Camera::GetInverseViewProjectionMatrix() const
	{
		return m_invViewProjection;
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
