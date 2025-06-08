// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Assert.h"

#include "Core/Util/HashUtils.h"
#include "Core/Interfaces/INamedObject.h"

#include "Renderer/Shaders/Common/CameraParams.h"


namespace gr
{
	class Camera
	{
	public:
		struct FrustumPlane final
		{
			glm::vec3 m_point;
			glm::vec3 m_normal;
		};
		struct Frustum final
		{
			std::array<FrustumPlane, 6> m_planes{};
			glm::vec3 m_camWorldPos;
		};
		class View final
		{
		public:
			const gr::RenderDataID m_cameraRenderDataID;

			enum Face : uint8_t // Corresponds to the ordering of cubemap view matrices
			{
				Default = 0,

				XPos = 0,
				XNeg = 1,
				YPos = 2,
				YNeg = 3,
				ZPos = 4,
				ZNeg = 5,

				Face_Count = 6
			} const m_face;
			static constexpr std::array<char const*, Face_Count> k_faceNames =
			{
				"Default/XPos",
				"XNeg",
				"YPos",
				"YNeg",
				"ZPos",
				"ZNeg"
			};
			SEStaticAssert(k_faceNames.size() == Face_Count, "Face names and count are out of sync");


		public:
			View(gr::RenderDataID renderDataID, Face face)
				: m_cameraRenderDataID(renderDataID)
				, m_face(face)
			{
			}


			View(gr::RenderDataID renderDataID)
				: View(renderDataID, gr::Camera::View::Face::Default)
			{
			}


			View(gr::RenderDataID renderDataID, uint8_t faceIdx)
				: View(renderDataID, static_cast<gr::Camera::View::Face>(faceIdx))
			{
				SEAssert(faceIdx < 6, "Face index is out of bounds");
			}


			bool operator==(View const& rhs) const noexcept
			{
				return m_cameraRenderDataID == rhs.m_cameraRenderDataID && 
					m_face == rhs.m_face;
			}


			bool operator<(View const& rhs) const noexcept
			{
				if (m_cameraRenderDataID == rhs.m_cameraRenderDataID)
				{
					return m_face < rhs.m_face;
				}
				return m_cameraRenderDataID < rhs.m_cameraRenderDataID;
			}
		};


	public:
		struct Config final
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


	public:
		struct RenderData final
		{
			gr::Camera::Config m_cameraConfig;

			// We compute this once on the main thread, and then pass for reuse on the render thread
			CameraData m_cameraParams; 

			gr::TransformID m_transformID;

			char m_cameraName[core::INamedObject::k_maxNameLength];
		};

		static uint8_t NumViews(gr::Camera::RenderData const& camData);


	public:
		static std::vector<glm::mat4> BuildAxisAlignedCubeViewMatrices(glm::vec3 const& centerPos);

		static std::vector<glm::mat4> BuildCubeViewMatrices(
			glm::vec3 const& centerPos, 
			glm::vec3 const& right,		// X
			glm::vec3 const& up,		// Y
			glm::vec3 const& forward);	// Z

		static std::vector<glm::mat4> BuildCubeInvViewMatrices(
			glm::vec3 const& centerPos,
			glm::vec3 const& right,		// X
			glm::vec3 const& up,		// Y
			glm::vec3 const& forward);	// Z
		
		static std::vector<glm::mat4> BuildCubeViewProjectionMatrices(
			std::vector<glm::mat4> const& viewMats, glm::mat4 const& projection);

		static std::vector<glm::mat4> BuildCubeInvViewProjectionMatrices(
			std::vector<glm::mat4> const& viewProjMats);

		static glm::mat4 BuildPerspectiveProjectionMatrix(float yFOV, float aspectRatio, float nearDist, float farDist);

		static glm::mat4 BuildOrthographicProjectionMatrix(
			float left, float right, float bottom, float top, float nearDist, float farDist);
		static glm::mat4 BuildOrthographicProjectionMatrix(
			glm::vec4 orthoLeftRightBotTop, float nearDist, float farDist);

		static float ComputeEV100FromExposureSettings(
			float aperture, float shutterSpeed, float sensitivity, float exposureCompensation);

		static float ComputeExposure(float ev100);

		static Frustum BuildWorldSpaceFrustumData(glm::vec3 camWorldPos, glm::mat4 const& invViewProjection);
		static Frustum BuildWorldSpaceFrustumData(glm::vec3 camWorldPos, glm::mat4 const& projection, glm::mat4 const& view);
	};
}


// Hash functions for our gr::Camera::View, to allow it to be used as a key in an associative container
template<>
struct std::hash<gr::Camera::View>
{
	std::size_t operator()(gr::Camera::View const& view) const noexcept
	{
		size_t result = 0;
		util::AddDataToHash(result, view.m_cameraRenderDataID);
		util::AddDataToHash(result, view.m_face);
		return result;
	}
};


template<>
struct std::hash<gr::Camera::View const>
{
	std::size_t operator()(gr::Camera::View const& view) const noexcept
	{
		size_t result = 0;
		util::AddDataToHash(result, view.m_cameraRenderDataID);
		util::AddDataToHash(result, view.m_face);
		return result;
	}
};