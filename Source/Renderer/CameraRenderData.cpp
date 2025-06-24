// © 2023 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "TransformRenderData.h"


namespace gr
{
	// Computes the camera's EV100 from exposure settings
	// aperture in f-stops
	// shutterSpeed in seconds
	// sensitivity in ISO
	// From Google Filament: https://google.github.io/filament/Filament.md.html#listing_fragmentexposure
	float Camera::ComputeEV100FromExposureSettings(
		float aperture, float shutterSpeed, float sensitivity, float exposureCompensation)
	{
		// EV_100	= log2((aperture^2)/shutterSpeed) - log2(sensitivity/100) 
		//			= log2(((aperture^2)/shutterSpeed) / (sensitivity/100))
		// We rearrange here to save a division:
		return log2((aperture * aperture) / shutterSpeed * 100.0f / sensitivity) - exposureCompensation;
	}


	// Computes the exposure normalization factor from the camera's EV100
	// ev100 computed via GetEV100FromExposureSettings
	// Based on Google Filament: https://google.github.io/filament/Filament.md.html#listing_fragmentexposure
	float Camera::ComputeExposure(float ev100)
	{
		// Note: Denominator approaches 0 as ev100 -> -inf (and is practically 0 as ev100 -> -10)
		return 1.0f / std::max((std::pow(2.0f, ev100) * 1.2f), FLT_MIN);
	}


	uint8_t Camera::NumViews(gr::Camera::RenderData const& camData)
	{
		return camData.m_cameraConfig.m_projectionType == Camera::Config::ProjectionType::PerspectiveCubemap ? 6 : 1;
	}


	std::vector<glm::mat4> Camera::BuildAxisAlignedCubeViewMatrices(glm::vec3 const& centerPos)
	{
		return BuildCubeViewMatrices(
			centerPos, gr::Transform::WorldAxisX, gr::Transform::WorldAxisY, gr::Transform::WorldAxisZ);
	}


	std::vector<glm::mat4> Camera::BuildCubeViewMatrices(
		glm::vec3 const& centerPos,
		glm::vec3 const& right,		// X
		glm::vec3 const& up,		// Y
		glm::vec3 const& forward)	// Z
	{
		std::vector<glm::mat4> cubeView;
		cubeView.reserve(6);

		cubeView.emplace_back(glm::lookAt( // X+
			centerPos,				// eye
			centerPos + right,		// center: Position the camera is looking at
			up));					// Normalized camera up vector
		cubeView.emplace_back(glm::lookAt( // X-
			centerPos,
			centerPos - right,
			up));

		cubeView.emplace_back(glm::lookAt( // Y+
			centerPos,
			centerPos + up,
			forward));
		cubeView.emplace_back(glm::lookAt( // Y-
			centerPos,
			centerPos - up,
			-forward));

		// In both OpenGL and DX12, cubemaps use a LHCS. SaberEngine uses a RHCS.
		// Here, we supply our coordinates w.r.t a LHCS by multiplying the Z direction (i.e. the point we're looking at)
		// by -1. In our shaders we must also transform our RHCS sample directions to LHCS.
		cubeView.emplace_back(glm::lookAt( // Z+
			centerPos,
			centerPos - forward, // * -1
			up));
		cubeView.emplace_back(glm::lookAt( // Z-
			centerPos,
			centerPos + forward, // * -1
			up));

		return cubeView;
	}


	std::vector<glm::mat4> Camera::BuildCubeInvViewMatrices(
		glm::vec3 const& centerPos,
		glm::vec3 const& right,		// X
		glm::vec3 const& up,		// Y
		glm::vec3 const& forward)
	{
		std::vector<glm::mat4> result = BuildCubeViewMatrices(centerPos, right, up, forward);

		for (glm::mat4& view : result)
		{
			view = glm::inverse(view);
		}

		return result;
	}


	std::vector<glm::mat4> Camera::BuildCubeViewProjectionMatrices(
		std::vector<glm::mat4> const& viewMats, glm::mat4 const& projection)
	{
		std::vector<glm::mat4> viewProjMats;
		viewProjMats.reserve(6);
		for (uint8_t faceIdx = 0; faceIdx < 6; faceIdx++)
		{
			viewProjMats.emplace_back(projection * viewMats[faceIdx]);
		}
		return viewProjMats;
	}


	std::vector<glm::mat4> Camera::BuildCubeInvViewProjectionMatrices(std::vector<glm::mat4> const& viewProjMats)
	{
		std::vector<glm::mat4> invViewProjMats;
		invViewProjMats.reserve(6);
		for (uint8_t faceIdx = 0; faceIdx < 6; faceIdx++)
		{
			invViewProjMats.emplace_back(glm::inverse(viewProjMats[faceIdx]));
		}
		return invViewProjMats;
	}


	glm::mat4 Camera::BuildPerspectiveProjectionMatrix(float yFOV, float aspectRatio, float nearDist, float farDist)
	{
		return glm::perspective(yFOV, aspectRatio, nearDist, farDist);
	}


	glm::mat4 Camera::BuildOrthographicProjectionMatrix(
		float left, float right, float bottom, float top, float nearDist, float farDist)
	{
		return glm::ortho(left, right, bottom, top, nearDist, farDist);
	}


	glm::mat4 Camera::BuildOrthographicProjectionMatrix(glm::vec4 orthoLeftRightBotTop, float nearDist, float farDist)
	{
		return BuildOrthographicProjectionMatrix(
			orthoLeftRightBotTop.x, 
			orthoLeftRightBotTop.y, 
			orthoLeftRightBotTop.z, 
			orthoLeftRightBotTop.w, 
			nearDist, 
			farDist);
	}


	gr::Camera::Frustum::Frustum(glm::vec3 camWorldPos, glm::mat4 const& invViewProjection)
	{
		// Convert cube in NDC space to world space
		glm::vec4 farTL = invViewProjection * glm::vec4(-1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBL = invViewProjection * glm::vec4(-1.f, -1.f, 1.f, 1.f);
		glm::vec4 farTR = invViewProjection * glm::vec4(1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBR = invViewProjection * glm::vec4(1.f, -1.f, 1.f, 1.f);
		glm::vec4 nearTL = invViewProjection * glm::vec4(-1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBL = invViewProjection * glm::vec4(-1.f, -1.f, 0.f, 1.f);
		glm::vec4 nearTR = invViewProjection * glm::vec4(1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBR = invViewProjection * glm::vec4(1.f, -1.f, 0.f, 1.f);

		farTL /= farTL.w;
		farBL /= farBL.w;
		farTR /= farTR.w;
		farBR /= farBR.w;
		nearTL /= nearTL.w;
		nearBL /= nearBL.w;
		nearTR /= nearTR.w;
		nearBR /= nearBR.w;

		// Store the frustum corners:
		m_corners[0] = farTL.xyz;
		m_corners[1] = farBL.xyz;
		m_corners[2] = farTR.xyz;
		m_corners[3] = farBR.xyz;
		m_corners[4] = nearTL.xyz;
		m_corners[5] = nearBL.xyz;
		m_corners[6] = nearTR.xyz;
		m_corners[7] = nearBR.xyz;

		// Near face (Behind the camera)
		m_points[0] = nearBL;
		m_edgeDirections[0] = glm::normalize(nearBR.xyz - nearBL.xyz);
		m_normals[0] = glm::normalize(glm::cross(m_edgeDirections[0], (nearTL.xyz - nearBL.xyz)));
		
		// Far face (beyond the far plane)
		m_points[1] = farBR;
		m_edgeDirections[1] = glm::normalize(farBL.xyz - farBR.xyz);
		m_normals[1] = glm::normalize(glm::cross(m_edgeDirections[1], (farTR.xyz - farBR.xyz)));
		
		// Left face
		m_points[2] = farBL;
		m_edgeDirections[2] = glm::normalize(nearBL.xyz - farBL.xyz);
		m_normals[2] = glm::normalize(glm::cross(m_edgeDirections[2], (farTL.xyz - farBL.xyz)));
		
		// Right face
		m_points[3] = nearBR;
		m_edgeDirections[3] = glm::normalize(farBR.xyz - nearBR.xyz);
		m_normals[3] = glm::normalize(glm::cross(m_edgeDirections[3], (nearTR.xyz - nearBR.xyz)));
		
		// Top face
		m_points[4] = nearTL;
		m_edgeDirections[4] = glm::normalize(nearTR.xyz - nearTL.xyz);
		m_normals[4] = glm::normalize(glm::cross(m_edgeDirections[4], (farTL.xyz - nearTL.xyz)));
		
		// Bottom face
		m_points[5] = farBL;
		m_edgeDirections[5] = glm::normalize(farBR.xyz - farBL.xyz);
		m_normals[5] = glm::normalize(glm::cross(m_edgeDirections[5], (nearBL.xyz - farBL.xyz)));		

		m_camPosition = camWorldPos;
	}
}