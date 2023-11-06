// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "ParameterBlock.h"
#include "NamedObject.h"
#include "Transformable.h"
#include "TextureTarget.h"
#include "Updateable.h"


namespace gr
{
	class Camera final : public virtual en::NamedObject, public virtual fr::Transformable, public virtual en::Updateable
	{
	public:
		struct Config
		{
			enum class ProjectionType
			{
				Perspective,
				Orthographic,
				PerspectiveCubemap // For rendering cubemaps: Has 6 view matrices
			} m_projectionType = ProjectionType::Perspective;
			
			float m_yFOV = static_cast<float>(std::numbers::pi) / 2.0f; // In radians; 0 if orthographic

			float m_near = 1.0f;
			float m_far = 100.0f;
			float m_aspectRatio = 1.0f; // == width / height

			// Orthographic properties:
			glm::vec4 m_orthoLeftRightBotTop = glm::vec4(-5.f, 5.f, -5.f, 5.f);

			// Sensor properties:
			// f/stops: == focal length / diameter of aperture (entrance pupil). Commonly 1.4, 2, 2.8, 4, 5.6, 8, 11, 16
			float m_aperture = 0.2f; // f/stops
			float m_shutterSpeed = 0.01f; // Seconds
			float m_sensitivity = 250.f; // ISO
			float m_exposureCompensation = 0.f; // f/stops
			// TODO: Add a lens size, and compute the aperture from that

			float m_bloomStrength = 0.2f;
			glm::vec2 m_bloomRadius = glm::vec2(1.f, 1.f);
			float m_bloomExposureCompensation = 0.f; // Overdrive bloom contribution
			bool m_deflickerEnabled = true;

			bool operator==(Config const& rhs) const
			{
				return m_projectionType == rhs.m_projectionType &&
					m_yFOV == rhs.m_yFOV &&
					m_near == rhs.m_near &&
					m_far == rhs.m_far &&
					m_aspectRatio == rhs.m_aspectRatio &&
					m_orthoLeftRightBotTop == rhs.m_orthoLeftRightBotTop &&
					m_aperture == rhs.m_aperture &&
					m_shutterSpeed == rhs.m_shutterSpeed &&
					m_sensitivity == rhs.m_sensitivity &&
					m_exposureCompensation == rhs.m_exposureCompensation &&
					m_bloomStrength == rhs.m_bloomStrength &&
					m_bloomRadius == rhs.m_bloomRadius &&
					m_bloomExposureCompensation == rhs.m_bloomExposureCompensation &&
					m_deflickerEnabled == rhs.m_deflickerEnabled;
			}
			
			bool operator!=(Config const& rhs) const
			{
				return !operator==(rhs);
			}
		};
		static_assert(sizeof(Config) == 72); // Don't forget to update operator== if the properties change

	public: // Shader parameter blocks:
		struct CameraParams 
		{
			glm::mat4 g_view;
			glm::mat4 g_invView;
			glm::mat4 g_projection;
			glm::mat4 g_invProjection;

			glm::mat4 g_viewProjection;
			glm::mat4 g_invViewProjection;

			glm::vec4 g_projectionParams; // .x = near, .y = far, .z = 1/near, .w = 1/far

			glm::vec4 g_exposureProperties; // .x = exposure, .y = ev100, .zw = unused 
			glm::vec4 g_bloomSettings; // .x = strength, .yz = XY radius, .w = bloom exposure compensation

			glm::vec4 g_cameraWPos; // .xyz = world pos, .w = unused

			static constexpr char const* const s_shaderName = "CameraParams"; // Not counted towards size of struct
		};


	public:
		static std::shared_ptr<gr::Camera> Create(
			std::string const& cameraName, Config const& camConfig, gr::Transform* parent);

		
		Camera(Camera&&) = default;
		Camera& operator=(Camera&&) = default;
		
		~Camera();

		void Destroy();

		void Update(const double stepTimeMs) override;

		float const FieldOfViewYRad() const;

		glm::vec2 NearFar() const; // TODO: RENAME "GetNearFar"
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

		Config const& GetCameraConfig() const;
		void SetCameraConfig(Config const& newConfig);

		std::shared_ptr<re::ParameterBlock> GetCameraParams() const;

		void SetAsMainCamera() const;

		void ShowImGuiWindow();


	private: // Use Create() instead
		Camera(std::string const& cameraName, Config const& camConfig, gr::Transform* parent);
		

	private:
		// Helper function: Configures the camera based on the cameraConfig. MUST be called at least once during setup
		void RecomputeMatrices(); // Returns true if parameters were recomputed, false otherwise
		void UpdateCameraParamBlockData();


	private:
		Config m_cameraConfig;

		std::vector<glm::mat4> m_view;
		std::vector<glm::mat4> m_invView;

		glm::mat4 m_projection;
		glm::mat4 m_invProjection;

		std::vector<glm::mat4> m_viewProjection;
		std::vector<glm::mat4> m_invViewProjection;

		bool m_matricesDirty;
		bool m_parameterBlockDirty;

		std::shared_ptr<re::ParameterBlock> m_cameraParamBlock;
		CameraParams m_cameraPBData;

	private:
		Camera() = delete;
		Camera(Camera const&) = delete;
		Camera& operator=(Camera const&) = delete;
	};


	inline float const Camera::FieldOfViewYRad() const
	{
		return m_cameraConfig.m_yFOV;
	}


	inline glm::vec2 Camera::NearFar() const
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


	inline Camera::Config const& Camera::GetCameraConfig() const
	{
		return m_cameraConfig;
	}


	inline std::shared_ptr<re::ParameterBlock> Camera::GetCameraParams() const
	{
		return m_cameraParamBlock;
	}
}
