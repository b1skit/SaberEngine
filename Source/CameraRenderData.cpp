// © 2023 Adam Badke. All rights reserved.
#include "Assert.h"
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


	gr::Camera::Frustum Camera::BuildWorldSpaceFrustumData(glm::vec3 camWorldPos, glm::mat4 const& invViewProjection)
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

		gr::Camera::Frustum frustum;

		// Near face (Behind the camera)
		frustum.m_planes[0].m_point = nearBL;
		frustum.m_planes[0].m_normal = glm::normalize(glm::cross((nearBR.xyz - nearBL.xyz), (nearTL.xyz - nearBL.xyz)));

		// Far face (beyond the far plane)
		frustum.m_planes[1].m_point = farBR;
		frustum.m_planes[1].m_normal = glm::normalize(glm::cross((farBL.xyz - farBR.xyz), (farTR.xyz - farBR.xyz)));

		// Left face
		frustum.m_planes[2].m_point = farBL;
		frustum.m_planes[2].m_normal = glm::normalize(glm::cross((nearBL.xyz - farBL.xyz), (farTL.xyz - farBL.xyz)));

		// Right face
		frustum.m_planes[3].m_point = nearBR;
		frustum.m_planes[3].m_normal = glm::normalize(glm::cross((farBR.xyz - nearBR.xyz), (nearTR.xyz - nearBR.xyz)));

		// Top face
		frustum.m_planes[4].m_point = nearTL;
		frustum.m_planes[4].m_normal = glm::normalize(glm::cross((nearTR.xyz - nearTL.xyz), (farTL.xyz - nearTL.xyz)));

		// Bottom face
		frustum.m_planes[5].m_point = farBL;
		frustum.m_planes[5].m_normal = glm::normalize(glm::cross((farBR.xyz - farBL.xyz), (nearBL.xyz - farBL.xyz)));

		frustum.m_camWorldPos = camWorldPos;

		return frustum;
	}


	gr::Camera::Frustum Camera::BuildWorldSpaceFrustumData(
		glm::vec3 camWorldPos, glm::mat4 const& projection, glm::mat4 const& view)
	{
		return BuildWorldSpaceFrustumData(camWorldPos, glm::inverse(projection * view));
	}
}