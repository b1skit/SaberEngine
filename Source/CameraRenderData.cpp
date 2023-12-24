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


	std::vector<glm::mat4> Camera::BuildCubeViewMatrices(glm::vec3 const& centerPos)
	{
		std::vector<glm::mat4> cubeView;
		cubeView.reserve(6);

		cubeView.emplace_back(glm::lookAt( // X+
			centerPos,								// eye
			centerPos + gr::Transform::WorldAxisX,	// center: Position the camera is looking at
			gr::Transform::WorldAxisY));			// Normalized camera up vector
		cubeView.emplace_back(glm::lookAt( // X-
			centerPos,
			centerPos - gr::Transform::WorldAxisX,
			gr::Transform::WorldAxisY));

		cubeView.emplace_back(glm::lookAt( // Y+
			centerPos,
			centerPos + gr::Transform::WorldAxisY,
			gr::Transform::WorldAxisZ));
		cubeView.emplace_back(glm::lookAt( // Y-
			centerPos,
			centerPos - gr::Transform::WorldAxisY,
			-gr::Transform::WorldAxisZ));

		// In both OpenGL and DX12, cubemaps use a LHCS. SaberEngine uses a RHCS.
		// Here, we supply our coordinates w.r.t a LHCS by multiplying the Z direction (i.e. the point we're looking at)
		// by -1. In our shaders we must also transform our RHCS sample directions to LHCS.
		cubeView.emplace_back(glm::lookAt( // Z+
			centerPos,
			centerPos - gr::Transform::WorldAxisZ, // * -1
			gr::Transform::WorldAxisY));
		cubeView.emplace_back(glm::lookAt( // Z-
			centerPos,
			centerPos + gr::Transform::WorldAxisZ, // * -1
			gr::Transform::WorldAxisY));

		return cubeView;
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
}