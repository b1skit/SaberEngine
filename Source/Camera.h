// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "NamedObject.h"
#include "ParameterBlock.h"
#include "RenderCommand.h"
#include "Transform.h"
#include "TextureTarget.h"
#include "Updateable.h"


namespace fr
{
	class Camera final : public virtual en::NamedObject, public virtual en::Updateable
	{
	public:
		static std::vector<glm::mat4> BuildCubeViewMatrices(glm::vec3 const& centerPos);

		static glm::mat4 BuildPerspectiveProjectionMatrix(float yFOV, float aspectRatio, float nearDist, float farDist);

		static glm::mat4 BuildOrthographicProjectionMatrix(
			float left, float right, float bottom, float top, float nearDist, float farDist);


	public:
		// Create a render camera, for backend graphics use. Not compatible with the ECS
		[[nodiscard]] static std::shared_ptr<fr::Camera> Create(
			std::string const& name, gr::Camera::Config const&, fr::Transform* parent); // DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		// ECS
		

		Camera(Camera&&) = default;
		Camera& operator=(Camera&&) = default;
		
		~Camera();

		void Destroy();

		void Update(const double stepTimeMs) override;

		float const FieldOfViewYRad() const;

		glm::vec2 GetNearFar() const;
		void SetNearFar(glm::vec2 const& nearFar);

		float GetAspectRatio() const;

		glm::mat4 GetViewMatrix() const;
		glm::mat4 const& GetInverseViewMatrix() const;

		glm::mat4 const& GetProjectionMatrix() const;
		glm::mat4 GetInverseProjectionMatrix() const;

		glm::mat4 GetViewProjectionMatrix() const;
		glm::mat4 GetInverseViewProjectionMatrix() const;

		std::vector<glm::mat4> const& GetCubeViewMatrices() const;
		std::vector<glm::mat4> const& GetCubeInvViewMatrices() const;
		std::vector<glm::mat4> const& GetCubeViewProjectionMatrices() const;
		std::vector<glm::mat4> const& GetCubeInvViewProjectionMatrices() const;
		
		float GetAperture() const;
		void SetAperture(float aperture);

		float GetShutterSpeed() const;
		void SetShutterSpeed(float shutterSpeed);

		float GetSensitivity() const;
		void SetSensitivity(float sensitivity);

		gr::Camera::Config const& GetCameraConfig() const;
		void SetCameraConfig(gr::Camera::Config const& newConfig);

		// Keep the pointer for convenience, but don't allow access to it!!!!!!!!!!
		fr::Transform* GetTransform(); // DEPRECATED
		fr::Transform const* GetTransform() const; // DEPRECATED

		std::shared_ptr<re::ParameterBlock> GetCameraParams() const; // DEPRECATED

		void SetAsMainCamera() const; // DEPRECATED ???????????

		void ShowImGuiWindow();

		
	private: // Use one of the Create methods instead
		// Camera component constructor: A Transform is allocated via the ECS and provided here
		Camera(std::string const& name, gr::Camera::Config const& camConfig, fr::Transform* transform, bool isComponent);

	private:
		// Helper function: Configures the camera based on the cameraConfig. MUST be called at least once during setup
		void RecomputeMatrices(); // Returns true if parameters were recomputed, false otherwise
		void UpdateCameraParamBlockData();


	private:
		fr::Transform* m_transform;
		const bool m_isComponent; // DEPRECATED

		// Render data
		gr::Camera::Config m_cameraConfig;

		std::vector<glm::mat4> m_view;
		std::vector<glm::mat4> m_invView;

		glm::mat4 m_projection;
		glm::mat4 m_invProjection;

		std::vector<glm::mat4> m_viewProjection;
		std::vector<glm::mat4> m_invViewProjection;

		bool m_matricesDirty;


		bool m_parameterBlockDirty; // DEPRECATED
		gr::Camera::CameraParams m_cameraPBData; // DEPRECATED
		std::shared_ptr<re::ParameterBlock> m_cameraParamBlock; // DEPRECATED
		

	private:
		Camera() = delete;
		Camera(Camera const&) = delete;
		Camera& operator=(Camera const&) = delete;
	};


	inline float const Camera::FieldOfViewYRad() const
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
	}


	inline float Camera::GetAspectRatio() const
	{
		return m_cameraConfig.m_aspectRatio;
	}


	inline glm::mat4 Camera::GetViewMatrix() const
	{
		return m_view[0];
	}


	inline glm::mat4 const& Camera::GetInverseViewMatrix() const
	{
		return m_invView[0];
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
		return m_viewProjection[0];
	}


	inline glm::mat4 Camera::GetInverseViewProjectionMatrix() const
	{
		return m_invViewProjection[0];
	}


	inline std::vector<glm::mat4> const& Camera::GetCubeViewMatrices() const
	{
		return m_view;
	}


	inline std::vector<glm::mat4> const& Camera::GetCubeInvViewMatrices() const
	{
		return m_invView;
	}


	inline std::vector<glm::mat4> const& Camera::GetCubeViewProjectionMatrices() const
	{
		return m_viewProjection;
	}
	

	inline std::vector<glm::mat4> const& Camera::GetCubeInvViewProjectionMatrices() const
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
	}

	inline float Camera::GetShutterSpeed() const
	{
		return m_cameraConfig.m_shutterSpeed;
	}


	inline void Camera::SetShutterSpeed(float shutterSpeed)
	{
		m_cameraConfig.m_shutterSpeed = shutterSpeed;
	}


	inline float Camera::GetSensitivity() const
	{
		return m_cameraConfig.m_sensitivity;
	}


	inline void Camera::SetSensitivity(float sensitivity)
	{
		m_cameraConfig.m_sensitivity = sensitivity;
	}


	inline gr::Camera::Config const& Camera::GetCameraConfig() const
	{
		return m_cameraConfig;
	}


	inline fr::Transform* Camera::GetTransform()
	{
		return m_transform;
	}


	inline fr::Transform const* Camera::GetTransform() const
	{
		return m_transform;
	}


	inline std::shared_ptr<re::ParameterBlock> Camera::GetCameraParams() const
	{
		return m_cameraParamBlock;
	}
}
