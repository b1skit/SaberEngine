#pragma once

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "NamedObject.h"
#include "Transformable.h"
#include "TextureTarget.h"
#include "ParameterBlock.h"
#include "Updateable.h"


namespace gr
{
	class Camera : public virtual en::NamedObject, public virtual fr::Transformable, public virtual en::Updateable
	{
	public:
		struct CameraConfig
		{
			enum class ProjectionType
			{
				Perspective,
				Orthographic
			} m_projectionType = ProjectionType::Perspective;

			float m_fieldOfView = 90.0f; // == 0 if orthographic. 
			// TODO: Store this in Radians (currently in degrees)
			// TODO: Convert this to vertical FOV, as per GLTF convention (currently horizontal)

			float m_near = 1.0f;
			float m_far = 100.0f;
			float m_aspectRatio = 1.0f; // == width / height

			// Orthographic properties:
			float m_orthoLeft = -5;
			float m_orthoRight = 5;
			float m_orthoBottom = -5;
			float m_orthoTop = 5;
			// TODO: Use a vec4 here

			// Image properties:
			float m_exposure = 1.0f; // TODO: Should this be stored here?
		};

	public:
		struct CameraParams
		{		
			// Shader parameter block
			glm::mat4 g_view;
			glm::mat4 g_invView;
			glm::mat4 g_projection;
			glm::mat4 g_invProjection;
			glm::mat4 g_viewProjection;
			glm::mat4 g_invViewProjection;

			glm::vec4 g_projectionParams; // .x = 1 (unused), .y = near, .z = far, .w = 1/far

			glm::vec3 g_cameraWPos;
			float padding0;

			// TODO: Implement exposure here
			// -> Currently using an exposure defined in the config to do tonemapping, and another exposure value when 
			// blooming...
			/*float g_exposure;*/
		};

	public:
		Camera(std::string const& cameraName, CameraConfig const& camConfig, gr::Transform* parent);
		~Camera() { Destroy(); }

		void Destroy();

		Camera(Camera&&) = default;
		Camera& operator=(Camera const&) = default;

		void Update(const double stepTimeMs) override;

		inline float const FieldOfView() const { return m_cameraConfig.m_fieldOfView; } // Degrees
		inline glm::vec2 NearFar() const { return glm::vec2(m_cameraConfig.m_near, m_cameraConfig.m_far); }

		inline glm::mat4 GetViewMatrix() { return glm::inverse(m_transform.GetGlobalMatrix(Transform::TRS)); }
		inline glm::mat4 GetInverseViewMatrix() { return m_transform.GetGlobalMatrix(Transform::TRS); }

		inline glm::mat4 const&	GetProjectionMatrix() const { return m_projection; }
		inline glm::mat4 GetInverseProjectionMatrix() const { return glm::inverse(m_projection); }

		inline glm::mat4 GetViewProjectionMatrix() { return m_projection * GetViewMatrix(); }
		inline glm::mat4 GetInverseViewProjectionMatrix() { return glm::inverse(GetViewProjectionMatrix()); }
		
		std::vector<glm::mat4> const& GetCubeViewProjectionMatrix();
		
		float& GetExposure() { return m_cameraConfig.m_exposure; }
		float GetExposure() const { return m_cameraConfig.m_exposure; }

		void SetCameraConfig(CameraConfig const& newConfig);

		inline std::shared_ptr<re::ParameterBlock> GetCameraParams() const {return m_cameraParamBlock; }

	private:
		// Helper function: Configures the camera based on the cameraConfig. MUST be called at least once during setup
		void Initialize();
		void UpdateCameraParamBlockData();

		std::vector<glm::mat4> const& GetCubeViewMatrix(); // TODO: Recompute this if the camera has moved

	private:
		CameraConfig m_cameraConfig;

		glm::mat4 m_view;
		glm::mat4 m_projection;

		std::vector<glm::mat4> m_cubeView;
		std::vector<glm::mat4> m_cubeViewProjection;

		std::shared_ptr<re::ParameterBlock> m_cameraParamBlock;
		CameraParams m_cameraPBData;

	private:
		Camera() = delete;
	};


}
