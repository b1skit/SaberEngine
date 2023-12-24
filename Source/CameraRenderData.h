// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "TransformRenderData.h"


namespace gr
{
	class Camera
	{
	public:
		static std::vector<glm::mat4> BuildCubeViewMatrices(glm::vec3 const& centerPos);

		static glm::mat4 BuildPerspectiveProjectionMatrix(float yFOV, float aspectRatio, float nearDist, float farDist);

		static glm::mat4 BuildOrthographicProjectionMatrix(
			float left, float right, float bottom, float top, float nearDist, float farDist);

		static float ComputeEV100FromExposureSettings(
			float aperture, float shutterSpeed, float sensitivity, float exposureCompensation);
			
		static float ComputeExposure(float ev100);


	public:
		struct Config
		{
			enum class ProjectionType
			{
				Perspective,
				Orthographic,

				PerspectiveCubemap

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


		struct RenderData
		{
			// Much less data to copy if we construct the CameraParams on the render thread
			gr::Camera::Config m_cameraConfig;

			// ECS_CONVERSION TODO: THIS IS JUST A HACK TO GET THINGS WORKING... NEED TO DECIDE THE BEST WAY
			// TO HANDLE THIS
			gr::Camera::CameraParams m_cameraParams;

			gr::TransformID m_transformID;
		};
	};
}