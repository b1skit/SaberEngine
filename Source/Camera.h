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
		struct CameraConfig
		{
			enum class ProjectionType
			{
				Perspective,
				Orthographic
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
			float m_bloomExposureCompensation = 3.f; // Overdrive emissive
			// TODO: Add a lens size, and compute the aperture from that

			bool operator==(CameraConfig const& rhs) const
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
					m_exposureCompensation == rhs.m_exposureCompensation;
			}
			
			bool operator!=(CameraConfig const& rhs) const
			{
				return !operator==(rhs);
			}
		};

	public:
		struct CameraParams // Shader parameter block
		{
			glm::mat4 g_view;
			glm::mat4 g_invView;
			glm::mat4 g_projection;
			glm::mat4 g_invProjection;
			glm::mat4 g_viewProjection;
			glm::mat4 g_invViewProjection;

			glm::vec4 g_projectionParams; // .x = 1 (unused), .y = near, .z = far, .w = 1/far

			glm::vec4 g_exposureProperties; // .x = exposure, .y = ev100, .z = bloom exposure compensation

			glm::vec3 g_cameraWPos;
			float padding0;

			static constexpr char const* const s_shaderName = "CameraParams"; // Not counted towards size of struct
		};

	public:
		static std::shared_ptr<gr::Camera> Create(
			std::string const& cameraName, CameraConfig const& camConfig, gr::Transform* parent);

		
		Camera(Camera&&) = default;
		Camera& operator=(Camera&&) = default;
		
		~Camera() { Destroy(); }

		void Destroy();

		void Update(const double stepTimeMs) override;

		inline float const FieldOfViewYRad() const { return m_cameraConfig.m_yFOV; }
		inline glm::vec2 NearFar() const { return glm::vec2(m_cameraConfig.m_near, m_cameraConfig.m_far); }
		inline float GetAspectRatio() const { return m_cameraConfig.m_aspectRatio; }

		inline glm::mat4 GetViewMatrix() { return glm::inverse(m_transform.GetGlobalMatrix(Transform::TRS)); }
		inline glm::mat4 const& GetInverseViewMatrix() { return m_transform.GetGlobalMatrix(Transform::TRS); }

		inline glm::mat4 const&	GetProjectionMatrix() const { return m_projection; }
		inline glm::mat4 GetInverseProjectionMatrix() const { return glm::inverse(m_projection); }

		inline glm::mat4 GetViewProjectionMatrix() { return m_projection * GetViewMatrix(); }
		inline glm::mat4 GetInverseViewProjectionMatrix() { return glm::inverse(GetViewProjectionMatrix()); }
		
		std::vector<glm::mat4> const& GetCubeViewProjectionMatrix();
		
		float GetAperture() const { return m_cameraConfig.m_aperture; }
		void SetAperture(float aperture) { m_cameraConfig.m_aperture = aperture; }

		float GetShutterSpeed() const { return m_cameraConfig.m_shutterSpeed; }
		void SetShutterSpeed(float shutterSpeed) { m_cameraConfig.m_shutterSpeed = shutterSpeed; }

		float GetSensitivity() const { return m_cameraConfig.m_sensitivity; }
		void SetSensitivity(float sensitivity) { m_cameraConfig.m_sensitivity = sensitivity; }

		void SetCameraConfig(CameraConfig const& newConfig);

		inline std::shared_ptr<re::ParameterBlock> GetCameraParams() const;

		void SetAsMainCamera() const;

		void ShowImGuiWindow();


	public: 
		// Static members:
		static std::vector<glm::mat4> GetCubeViewMatrix(glm::vec3 centerPos);


	private: // Use Create() instead
		Camera(std::string const& cameraName, CameraConfig const& camConfig, gr::Transform* parent);
		

	private:
		// Helper function: Configures the camera based on the cameraConfig. MUST be called at least once during setup
		void RecomputeProjectionMatrices(); // Returns true if parameters were recomputed, false otherwise
		void UpdateCameraParamBlockData();


	private:
		CameraConfig m_cameraConfig;
		bool m_projectionMatricesDirty;

		// TODO: Cache matrices, update them when they're dirty

		glm::mat4 m_projection;

		std::vector<glm::mat4> m_cubeView;
		std::vector<glm::mat4> m_cubeViewProjection;

		std::shared_ptr<re::ParameterBlock> m_cameraParamBlock;
		CameraParams m_cameraPBData;

		size_t m_cameraIdx; // Index in the global camera list

	private:
		Camera() = delete;
		Camera(Camera const&) = delete;
		Camera& operator=(Camera const&) = delete;
	};


	std::shared_ptr<re::ParameterBlock> Camera::GetCameraParams() const
	{
		return m_cameraParamBlock;
	}
}
