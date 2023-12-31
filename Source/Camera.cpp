// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Camera.h"
#include "ImGuiUtils.h"
#include "Transform.h"


namespace fr
{
	gr::Camera::CameraParams Camera::BuildCameraParams(fr::Camera const& camera)
	{
		gr::Camera::Config const& cameraConfig = camera.GetCameraConfig();
		fr::Transform const& transform = *camera.GetTransform();

		gr::Camera::CameraParams cameraParams{};

		glm::mat4 const& globalMatrix = transform.GetGlobalMatrix();
		cameraParams.g_view = glm::inverse(globalMatrix);
		cameraParams.g_invView = globalMatrix;

		switch (cameraConfig.m_projectionType)
		{
		case gr::Camera::Config::ProjectionType::Perspective:
		case gr::Camera::Config::ProjectionType::PerspectiveCubemap:
		{
			cameraParams.g_projection = gr::Camera::BuildPerspectiveProjectionMatrix(
				cameraConfig.m_yFOV,
				cameraConfig.m_aspectRatio,
				cameraConfig.m_near,
				cameraConfig.m_far);

			cameraParams.g_invProjection = glm::inverse(cameraParams.g_projection);
		}
		break;
		case gr::Camera::Config::ProjectionType::Orthographic:
		{
			cameraParams.g_projection = gr::Camera::BuildOrthographicProjectionMatrix(
				cameraConfig.m_orthoLeftRightBotTop.x,
				cameraConfig.m_orthoLeftRightBotTop.y,
				cameraConfig.m_orthoLeftRightBotTop.z,
				cameraConfig.m_orthoLeftRightBotTop.w,
				cameraConfig.m_near,
				cameraConfig.m_far);

			cameraParams.g_invProjection = glm::inverse(cameraParams.g_projection);
		}
		break;
		default: SEAssertF("Invalid projection type");
		}

		cameraParams.g_viewProjection = cameraParams.g_projection * cameraParams.g_view;
		cameraParams.g_invViewProjection = glm::inverse(cameraParams.g_viewProjection);

		// .x = near, .y = far, .z = 1/near, .w = 1/far
		cameraParams.g_projectionParams = glm::vec4(
			cameraConfig.m_near,
			cameraConfig.m_far,
			1.f / cameraConfig.m_near,
			1.f / cameraConfig.m_far);

		const float ev100 = gr::Camera::ComputeEV100FromExposureSettings(
			cameraConfig.m_aperture,
			cameraConfig.m_shutterSpeed,
			cameraConfig.m_sensitivity,
			cameraConfig.m_exposureCompensation);

		cameraParams.g_exposureProperties = glm::vec4(
			gr::Camera::ComputeExposure(ev100),
			ev100,
			0.f,
			0.f);

		const float bloomEV100 = gr::Camera::ComputeEV100FromExposureSettings(
			cameraConfig.m_aperture,
			cameraConfig.m_shutterSpeed,
			cameraConfig.m_sensitivity,
			cameraConfig.m_bloomExposureCompensation);

		cameraParams.g_bloomSettings = glm::vec4(
			cameraConfig.m_bloomStrength,
			cameraConfig.m_bloomRadius.x,
			cameraConfig.m_bloomRadius.y,
			gr::Camera::ComputeExposure(bloomEV100));

		cameraParams.g_cameraWPos = glm::vec4(transform.GetGlobalPosition().xyz, 0.f);

		return cameraParams;
	}


	Camera::Camera(gr::Camera::Config const& camConfig, fr::Transform const* transform)
		: m_transform(transform)
		, m_cameraConfig(camConfig)
		, m_isDirty(true)
	{
	}


	void Camera::SetCameraConfig(gr::Camera::Config const& newConfig)
	{
		if (newConfig != m_cameraConfig)
		{
			m_cameraConfig = newConfig;
			m_isDirty = true;
		}		
	}


	void Camera::ShowImGuiWindow(uint64_t uniqueID, fr::Transform& camTransform)
	{
		if (ImGui::CollapsingHeader(std::format("Modify##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			const std::string nearSliderLabel = std::format("Near plane distance##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(nearSliderLabel.c_str(), &m_cameraConfig.m_near, 0.f, 2.0f, "near = %.3f");

			const std::string farSliderLabel = std::format("Far plane distance##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(farSliderLabel.c_str(), &m_cameraConfig.m_far, 0.f, 1000.0f, "far = %.3f");

			ImGui::Text("1/far = %f", 1.f / m_cameraConfig.m_far);

			ImGui::Text("Sensor Properties");

			const std::string apertureLabel = std::format("Aperture (f/stops)##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(apertureLabel.c_str(), &m_cameraConfig.m_aperture, 0, 1.0f, "Aperture = %.3f");
			ImGui::SetItemTooltip("Expressed in f-stops. Controls how open/closed the aperture is. f-stops indicate the\n"
				"ratio of the lens' focal length to the diameter of the entrance pupil. High-values (f/16) indicate a\n"
				"small aperture,small values (f/1.4) indicate a wide aperture. Controls exposition and the depth of field");

			const std::string shutterSpeedLabel = std::format("Shutter Speed (seconds)##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(
				shutterSpeedLabel.c_str(), &m_cameraConfig.m_shutterSpeed, 0, 0.2f, "Shutter speed = %.3f");
			ImGui::SetItemTooltip("Expressed in seconds. Controls how long the aperture remains opened and the timing of\n"
				"the sensor shutter(s)). Controls exposition and motion blur.");

			const std::string sensitivityLabel = std::format("Sensitivity (ISO)##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(
				sensitivityLabel.c_str(), &m_cameraConfig.m_sensitivity, 0, 1000.0f, "Sensitivity = %.3f");
			ImGui::SetItemTooltip("Expressed in ISO. Controls how the light reaching the sensor is quantized. Controls\n"
				"exposition and the amount of noise.");

			// Effectively compute EV = log_2(N^2 / t)
			const float ev100 = gr::Camera::ComputeEV100FromExposureSettings(
				m_cameraConfig.m_aperture,
				m_cameraConfig.m_shutterSpeed,
				100.f, // ISO 100 sensitivity. 
				0.f); // No EC

			ImGui::Text("Exposure (EV_100) = %f", ev100);
			ImGui::SetItemTooltip("EV is in a base-2 logarithmic scale, a difference of 1 EV is called a stop. One\n"
				"positive stop (+1 EV) corresponds to a factor of two in luminance and one negative stop (-1 EV) \n"
				"corresponds to a factor of half in luminance. EV = log_2((N^2)/t, N = aperture, t = shutter speed.\n"
				"Note: EV is only a function of the aperture and shutter speed, but not the sensitivity. By\n"
				"convention, exposure values are defined for a sensitivity of ISO 100 (or EV_100), which is what we\n"
				"show here (i.e. With a sensitivity of 100, EV == EV_100).");

			const float evs = gr::Camera::ComputeEV100FromExposureSettings(
				m_cameraConfig.m_aperture,
				m_cameraConfig.m_shutterSpeed,
				m_cameraConfig.m_sensitivity,
				0.f); // No EC

			ImGui::Text("Exposure (EV_s) = %f", evs);
			ImGui::SetItemTooltip("EV_s is the exposure at a given sensitivity. By convention, exposure is defined\n"
				"for an ISO 100 sensitivity only");

			const std::string exposureCompensationLabel = std::format("Exposure compensation (EC)##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(
				exposureCompensationLabel.c_str(), &m_cameraConfig.m_exposureCompensation, -6.f, 6.0f, "EC = %.3f");
			ImGui::SetItemTooltip("Exposure compensation can be used to over/under-expose an image, for artistic control\n"
				"or to achieve proper exposure (e.g. snow can be exposed for as 18% middle-gray). EC is in f/stops:\n"
				"Increasing the EV is akin to closing down the lens aperture, reducing shutter speed, or reducing\n"
				"sensitivity. Higher EVs = darker images");

			const std::string boolStrengthLabel = std::format("Bloom strength##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(boolStrengthLabel.c_str(), &m_cameraConfig.m_bloomStrength, 0.f, 1.f, "Bloom strength = %.3f");


			static bool s_useRoundBlurRadius = true;
			m_isDirty |= ImGui::Checkbox(
				std::format("Round blur raduis?##{}", uniqueID).c_str(), &s_useRoundBlurRadius);
			if (!s_useRoundBlurRadius)
			{
				m_isDirty |= ImGui::SliderFloat(
					"Bloom radius width", &m_cameraConfig.m_bloomRadius.x, 1.f, 10.f);
				m_isDirty |= ImGui::SliderFloat(
					"Bloom radius height", &m_cameraConfig.m_bloomRadius.y, 1.f, 10.f);
			}
			else
			{
				const std::string bloomRadiusLabel = std::format("Bloom radius##{}", uniqueID);
				m_isDirty |= ImGui::SliderFloat(
					bloomRadiusLabel.c_str(), &m_cameraConfig.m_bloomRadius.x, 1.f, 10.f);
				m_cameraConfig.m_bloomRadius.y = m_cameraConfig.m_bloomRadius.x;
			}

			const std::string boolExposureCompensationLabel = std::format("Bloom exposure compensation (Bloom EC)##{}", uniqueID);
			m_isDirty |= ImGui::SliderFloat(
				boolExposureCompensationLabel.c_str(), &m_cameraConfig.m_bloomExposureCompensation, -6.f, 6.0f, "Bloom EC = %.3f");
			ImGui::SetItemTooltip("Independently expose the lens bloom contribution");

			m_isDirty |= ImGui::Checkbox(std::format("Enable bloom deflicker##{}", uniqueID).c_str(), &m_cameraConfig.m_deflickerEnabled);

			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader(std::format("Matrices##{}", uniqueID).c_str()))
		{
			ImGui::Indent();

			gr::Camera::CameraParams const& camParams = fr::Camera::BuildCameraParams(*this);

			util::DisplayMat4x4("View Matrix:", camParams.g_view);
			util::DisplayMat4x4("Inverse View Matrix:", camParams.g_invView);
			util::DisplayMat4x4("Projection Matrix:", camParams.g_projection);
			util::DisplayMat4x4("Inverse Projection Matrix:", camParams.g_invProjection);
			util::DisplayMat4x4("View Projection Matrix:", camParams.g_viewProjection);
			util::DisplayMat4x4("Inverse View Projection Matrix:", camParams.g_invViewProjection);

			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader(std::format("Transform##{}", uniqueID).c_str()))
		{
			ImGui::Indent();
			camTransform.ShowImGuiWindow();
			ImGui::Unindent();
		}
	}
}

