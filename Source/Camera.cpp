// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Camera.h"
#include "ImGuiUtils.h"
#include "Texture.h"
#include "Material.h"
#include "SceneManager.h"

using gr::Material;
using re::Texture;
using gr::Transform;
using std::shared_ptr;
using std::string;
using std::make_shared;
using glm::mat4;

namespace
{
	// Computes the camera's EV100 from exposure settings
	// aperture in f-stops
	// shutterSpeed in seconds
	// sensitivity in ISO
	// From Google Filament: https://google.github.io/filament/Filament.md.html#listing_fragmentexposure
	float ComputeEV100FromExposureSettings(float aperture, float shutterSpeed, float sensitivity, float exposureCompensation)
	{
		// EV_100	= log2((aperture^2)/shutterSpeed) - log2(sensitivity/100) 
		//			= log2(((aperture^2)/shutterSpeed) / (sensitivity/100))
		// We rearrange here to save a division:
		return log2((aperture * aperture) / shutterSpeed * 100.0f / sensitivity) - exposureCompensation;
	}


	// Computes the exposure normalization factor from the camera's EV100
	// ev100 computed via GetEV100FromExposureSettings
	// Based on Google Filament: https://google.github.io/filament/Filament.md.html#listing_fragmentexposure
	float ComputeExposure(float ev100)
	{
		// Note: Denominator approaches 0 as ev100 -> -inf (and is practically 0 as ev100 -> -10)
		return 1.0f / std::max((std::pow(2.0f, ev100) * 1.2f), FLT_MIN);
	}
}

namespace gr
{
	std::shared_ptr<gr::Camera> Camera::Create(
		std::string const& cameraName, CameraConfig const& camConfig, gr::Transform* parent)
	{
		std::shared_ptr<gr::Camera> newCamera = nullptr;
		newCamera.reset(new gr::Camera(cameraName, camConfig, parent));

		newCamera->m_cameraIdx = en::SceneManager::GetSceneData()->AddCamera(newCamera);

		return newCamera;
	}


	Camera::Camera(string const& cameraName, CameraConfig const& camConfig, Transform* parent)
		: NamedObject(cameraName)
		, Transformable(parent)
		, m_cameraConfig(camConfig)
		, m_projectionMatricesDirty(true)
	{
		m_cameraPBData = {}; // Initialize with a default struct: Updated later

		m_cameraParamBlock = re::ParameterBlock::Create(
			CameraParams::s_shaderName,
			m_cameraPBData, 
			re::ParameterBlock::PBType::Mutable);

		m_cubeView.reserve(6);
		RecomputeProjectionMatrices();
		UpdateCameraParamBlockData();
	}


	void Camera::Update(const double stepTimeMs)
	{
		// Note: We move the camera by modify its Transform directly
		RecomputeProjectionMatrices();
		UpdateCameraParamBlockData();
	}


	void Camera::UpdateCameraParamBlockData()
	{
		SEAssert("Camera parameter block has not been initialized yet", m_cameraParamBlock != nullptr);

		m_cameraPBData.g_view = GetViewMatrix();
		m_cameraPBData.g_invView = GetInverseViewMatrix();

		m_cameraPBData.g_projection = GetProjectionMatrix();
		m_cameraPBData.g_invProjection = GetInverseProjectionMatrix();

		m_cameraPBData.g_viewProjection = GetViewProjectionMatrix();
		m_cameraPBData.g_invViewProjection = GetInverseViewProjectionMatrix();

		// .x = 1 (unused), .y = near, .z = far, .w = 1/far
		m_cameraPBData.g_projectionParams = glm::vec4(
			1.f, 
			m_cameraConfig.m_near, 
			m_cameraConfig.m_far, 
			1.0f / m_cameraConfig.m_far);

		const float ev100 = ComputeEV100FromExposureSettings(
			m_cameraConfig.m_aperture, 
			m_cameraConfig.m_shutterSpeed, 
			m_cameraConfig.m_sensitivity, 
			m_cameraConfig.m_exposureCompensation);

		m_cameraPBData.g_exposureProperties = glm::vec4(
			ComputeExposure(ev100),
			ev100,
			0.f,
			0.f);

		const float bloomEV100 = ComputeEV100FromExposureSettings(
			m_cameraConfig.m_aperture,
			m_cameraConfig.m_shutterSpeed,
			m_cameraConfig.m_sensitivity,
			m_cameraConfig.m_bloomExposureCompensation);

		m_cameraPBData.g_bloomSettings = glm::vec4(
			m_cameraConfig.m_bloomStrength,
			m_cameraConfig.m_bloomRadius.x,
			m_cameraConfig.m_bloomRadius.y,
			ComputeExposure(bloomEV100));

		m_cameraPBData.g_cameraWPos = glm::vec4(GetTransform()->GetGlobalPosition().xyz, 0.f);

		m_cameraParamBlock->Commit(m_cameraPBData);
	}


	void Camera::RecomputeProjectionMatrices()
	{
		if (!m_projectionMatricesDirty)
		{
			return;
		}
		m_projectionMatricesDirty = false;

		if (m_cameraConfig.m_projectionType == CameraConfig::ProjectionType::Orthographic)
		{
			m_cameraConfig.m_yFOV = 0.0f;

			m_projection = glm::ortho
			(
				m_cameraConfig.m_orthoLeftRightBotTop.x,
				m_cameraConfig.m_orthoLeftRightBotTop.y,
				m_cameraConfig.m_orthoLeftRightBotTop.z,
				m_cameraConfig.m_orthoLeftRightBotTop.w,
				m_cameraConfig.m_near, 
				m_cameraConfig.m_far
			);
		}
		else
		{
			m_cameraConfig.m_orthoLeftRightBotTop = glm::vec4(0.f, 0.f, 0.f, 0.f);

			m_projection = glm::perspective
			(
				m_cameraConfig.m_yFOV,
				m_cameraConfig.m_aspectRatio, 
				m_cameraConfig.m_near, 
				m_cameraConfig.m_far
			);
		}
	}


	void Camera::Destroy()
	{
		m_cameraParamBlock = nullptr;
	}


	std::vector<glm::mat4> Camera::BuildCubeViewMatrices(glm::vec3 const& centerPos)
	{
		std::vector<glm::mat4> cubeView;
		cubeView.reserve(6);

		cubeView.emplace_back(glm::lookAt( // X+
			centerPos,							// eye
			centerPos + Transform::WorldAxisX,	// center: Position the camera is looking at
			Transform::WorldAxisY));			// Normalized camera up vector
		cubeView.emplace_back(glm::lookAt( // X-
			centerPos,
			centerPos - Transform::WorldAxisX,
			Transform::WorldAxisY));

		cubeView.emplace_back(glm::lookAt( // Y+
			centerPos,
			centerPos + Transform::WorldAxisY,
			Transform::WorldAxisZ));
		cubeView.emplace_back(glm::lookAt( // Y-
			centerPos,
			centerPos - Transform::WorldAxisY,
			-Transform::WorldAxisZ));

		// In both OpenGL and DX12, cubemaps use a LHCS. SaberEngine uses a RHCS.
		// Here, we supply our coordinates w.r.t a LHCS by multiplying the Z direction (i.e. the point we're looking at)
		// by -1. In our shaders we must also transform our RHCS sample directions to LHCS.
		cubeView.emplace_back(glm::lookAt( // Z+
			centerPos,
			centerPos - Transform::WorldAxisZ, // * -1
			Transform::WorldAxisY));
		cubeView.emplace_back(glm::lookAt( // Z-
			centerPos,
			centerPos + Transform::WorldAxisZ, // * -1
			Transform::WorldAxisY));

		return cubeView;
	}


	std::vector<glm::mat4> const& Camera::GetCubeViewProjectionMatrix()
	{
		m_cubeViewProjection.clear();

		std::vector<glm::mat4> const& cubeViews = BuildCubeViewMatrices(m_transform.GetGlobalPosition());

		m_cubeViewProjection.emplace_back(m_projection * cubeViews[0]);
		m_cubeViewProjection.emplace_back(m_projection * cubeViews[1]);
		m_cubeViewProjection.emplace_back(m_projection * cubeViews[2]);
		m_cubeViewProjection.emplace_back(m_projection * cubeViews[3]);
		m_cubeViewProjection.emplace_back(m_projection * cubeViews[4]);
		m_cubeViewProjection.emplace_back(m_projection * cubeViews[5]);

		return m_cubeViewProjection;
	}


	void Camera::SetCameraConfig(CameraConfig const& newConfig)
	{
		if (newConfig != m_cameraConfig)
		{
			m_cameraConfig = newConfig;
			m_projectionMatricesDirty = true;
		}		
	}


	void Camera::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(GetName().c_str(), ImGuiTreeNodeFlags_None))
		{
			const string nearSliderLabel = "Near plane distance##" + GetName(); // Prevent ID collisions; "##" hides whatever follows
			m_projectionMatricesDirty |= ImGui::SliderFloat(nearSliderLabel.c_str(), &m_cameraConfig.m_near, 0.f, 2.0f, "near = %.3f");

			const string farSliderLabel = "Far plane distance##" + GetName(); // Prevent ID collisions; "##" hides whatever follows
			m_projectionMatricesDirty |= ImGui::SliderFloat(farSliderLabel.c_str(), &m_cameraConfig.m_far, 0.f, 1000.0f, "far = %.3f");

			ImGui::Text("1/far = %f", 1.f / m_cameraConfig.m_far);

			ImGui::Text("Sensor Properties");

			const string apertureLabel = "Aperture (f/stops)##" + GetName();
			ImGui::SliderFloat(apertureLabel.c_str(), &m_cameraConfig.m_aperture, 0, 1.0f, "Aperture = %.3f");
			ImGui::SetItemTooltip("Expressed in f-stops. Controls how open/closed the aperture is. f-stops indicate the\n"
				"ratio of the lens' focal length to the diameter of the entrance pupil. High-values (f/16) indicate a\n"
				"small aperture,small values (f/1.4) indicate a wide aperture. Controls exposition and the depth of field");

			const string shutterSpeedLabel = "Shutter Speed (seconds)##" + GetName();
			ImGui::SliderFloat(shutterSpeedLabel.c_str(), &m_cameraConfig.m_shutterSpeed, 0, 0.2f, "Shutter speed = %.3f");
			ImGui::SetItemTooltip("Expressed in seconds. Controls how long the aperture remains opened and the timing of\n"
				"the sensor shutter(s)). Controls exposition and motion blur.");

			const string sensitivityLabel = "Sensitivity (ISO)##" + GetName();
			ImGui::SliderFloat(sensitivityLabel.c_str(), &m_cameraConfig.m_sensitivity, 0, 1000.0f, "Sensitivity = %.3f");
			ImGui::SetItemTooltip("Expressed in ISO. Controls how the light reaching the sensor is quantized. Controls\n"
				"exposition and the amount of noise.");

			// Effectively compute EV = log_2(N^2 / t)
			const float ev100 = ComputeEV100FromExposureSettings(
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

			const float evs = ComputeEV100FromExposureSettings(
				m_cameraConfig.m_aperture,
				m_cameraConfig.m_shutterSpeed,
				m_cameraConfig.m_sensitivity,
				0.f); // No EC

			ImGui::Text("Exposure (EV_s) = %f", evs);
			ImGui::SetItemTooltip("EV_s is the exposure at a given sensitivity. By convention, exposure is defined\n"
				"for an ISO 100 sensitivity only");

			const string exposureCompensationLabel = "Exposure compensation (EC)##" + GetName();
			ImGui::SliderFloat(exposureCompensationLabel.c_str(), &m_cameraConfig.m_exposureCompensation, -6.f, 6.0f, "EC = %.3f");
			ImGui::SetItemTooltip("Exposure compensation can be used to over/under-expose an image, for artistic control\n"
				"or to achieve proper exposure (e.g. snow can be exposed for as 18% middle-gray). EC is in f/stops:\n"
				"Increasing the EV is akin to closing down the lens aperture, reducing shutter speed, or reducing\n"
				"sensitivity. Higher EVs = darker images");

			const string boolStrengthLabel = "Bloom strength##" + GetName();
			ImGui::SliderFloat(boolStrengthLabel.c_str(), &m_cameraConfig.m_bloomStrength, 0.f, 1.f, "Bloom strength = %.3f");

			
			static bool s_useRoundBlurRadius = true;
			ImGui::Checkbox(std::format("Round blur raduis?##{}", GetUniqueID()).c_str(), &s_useRoundBlurRadius);
			if (!s_useRoundBlurRadius)
			{
				ImGui::SliderFloat("Bloom radius width", &m_cameraConfig.m_bloomRadius.x, 1.f, 10.f);
				ImGui::SliderFloat("Bloom radius height", &m_cameraConfig.m_bloomRadius.y, 1.f, 10.f);
			}
			else
			{
				const string bloomRadiusLabel = std::format("Bloom radius##{}", GetUniqueID());
				ImGui::SliderFloat(bloomRadiusLabel.c_str(), &m_cameraConfig.m_bloomRadius.x, 1.f, 10.f);
				m_cameraConfig.m_bloomRadius.y = m_cameraConfig.m_bloomRadius.x;
			}

			const string boolExposureCompensationLabel = "Bloom exposure compensation (Bloom EC)##" + GetName();
			ImGui::SliderFloat(boolExposureCompensationLabel.c_str(), &m_cameraConfig.m_bloomExposureCompensation, -6.f, 6.0f, "Bloom EC = %.3f");
			ImGui::SetItemTooltip("Independently expose the lens bloom contribution");

			ImGui::Checkbox(std::format("Enable bloom deflicker##{}", GetUniqueID()).c_str(), &m_cameraConfig.m_deflickerEnabled);

			util::DisplayMat4x4("View Matrix:", GetViewMatrix());

			const glm::mat4x4 invView = GetInverseViewMatrix();
			util::DisplayMat4x4("Inverse View Matrix:", invView);

			util::DisplayMat4x4("Projection Matrix:", m_projection);

			const glm::mat4x4 invProj = GetInverseProjectionMatrix();
			util::DisplayMat4x4("Inverse Projection Matrix:", invProj);

			const glm::mat4x4 viewProj = GetViewProjectionMatrix();
			util::DisplayMat4x4("View Projection Matrix:", viewProj);

			const glm::mat4x4 invViewProj = GetInverseViewProjectionMatrix();
			util::DisplayMat4x4("Inverse View Projection Matrix:", invViewProj);
		}
	}


	void Camera::SetAsMainCamera() const
	{
		en::SceneManager::Get()->SetMainCameraIdx(m_cameraIdx);
	}
}

