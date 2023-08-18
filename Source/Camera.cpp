// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"


namespace gr
{
	using gr::Material;
	using re::Texture;
	using gr::Transform;
	using std::shared_ptr;
	using std::string;
	using std::make_shared;
	using glm::mat4;


	Camera::Camera(string const& cameraName, CameraConfig const& camConfig, Transform* parent)
		: NamedObject(cameraName)
		, Transformable(parent)
		, m_cameraConfig(camConfig)
		, m_isDirty(true)
	{
		m_cubeView.reserve(6);
		UpdateCameraParamBlockData();
	}


	void Camera::Update(const double stepTimeMs)
	{
		UpdateCameraParamBlockData();
	}


	void Camera::UpdateCameraParamBlockData()
	{
		ComputeParameters();

		SEAssert("Camera parameter block has not been initialized yet", m_cameraParamBlock != nullptr);

		m_cameraPBData.g_view = GetViewMatrix();
		m_cameraPBData.g_invView = GetInverseViewMatrix();

		m_cameraPBData.g_projection = GetProjectionMatrix();
		m_cameraPBData.g_invProjection = GetInverseProjectionMatrix();

		m_cameraPBData.g_viewProjection = GetViewProjectionMatrix();
		m_cameraPBData.g_invViewProjection = GetInverseViewProjectionMatrix();

		// .x = 1 (unused), .y = near, .z = far, .w = 1/far
		m_cameraPBData.g_projectionParams = 
			glm::vec4(1.f, m_cameraConfig.m_near, m_cameraConfig.m_far, 1.0f / m_cameraConfig.m_far);

		m_cameraPBData.g_cameraWPos = GetTransform()->GetGlobalPosition();

		m_cameraParamBlock->Commit(m_cameraPBData);

		// TODO: It's possible to update the camera params multiple times in a frame if SetCameraConfig is called by
		// another object in the Updateable list.
		// eg. Light::Update -> SetCameraConfig
		// -> Need to switch to a scene graph representation so the update order each frame is determinate
	}


	void Camera::ComputeParameters()
	{
		if (!m_isDirty)
		{
			return;
		}
		m_isDirty = false;

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

		// Initialize the param block pointer first:
		if (m_cameraParamBlock == nullptr)
		{
			m_cameraParamBlock = re::ParameterBlock::Create(
				CameraParams::s_shaderName,
				m_cameraPBData, // Initialize with a default struct: Updated in UpdateCameraParamBlockData()
				re::ParameterBlock::PBType::Mutable);
		}
	}


	void Camera::Destroy()
	{
		m_cameraParamBlock = nullptr;
	}


	std::vector<glm::mat4> Camera::GetCubeViewMatrix(glm::vec3 centerPos)
	{
		std::vector<glm::mat4> cubeView;
		cubeView.reserve(6);

		cubeView.emplace_back(glm::lookAt( // X+
			centerPos,							// eye
			centerPos + Transform::WorldAxisX,	// center: Position the camera is looking at
			-Transform::WorldAxisY));									// Normalized camera up vector
		cubeView.emplace_back(glm::lookAt( // X-
			centerPos,
			centerPos - Transform::WorldAxisX,
			-Transform::WorldAxisY));

		// Note: The cubemap Y matrices generated here are flipped to (partially) compensate for our use of the  
		// uv (0,0) = top-left convention we've forced in OpenGL
		cubeView.emplace_back(glm::lookAt( // Y+
			centerPos,
			centerPos - Transform::WorldAxisY,
			-Transform::WorldAxisZ));
		cubeView.emplace_back(glm::lookAt( // Y-
			centerPos,
			centerPos + Transform::WorldAxisY,
			Transform::WorldAxisZ));

		cubeView.emplace_back(glm::lookAt( // Z+
			centerPos,
			centerPos + Transform::WorldAxisZ,
			-Transform::WorldAxisY));
		cubeView.emplace_back(glm::lookAt( // Z-
			centerPos,
			centerPos - Transform::WorldAxisZ,
			-Transform::WorldAxisY));

		return cubeView;
	}


	std::vector<glm::mat4> const& Camera::GetCubeViewProjectionMatrix()
	{
		m_cubeViewProjection.clear();

		std::vector<glm::mat4> const& cubeViews = GetCubeViewMatrix(m_transform.GetGlobalPosition());

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
		m_cameraConfig = newConfig;
		ComputeParameters();
	}


	void Camera::ShowImGuiWindow()
	{
		ImGui::Text("Name: \"%s\"", GetName().c_str());
		m_isDirty |= ImGui::SliderFloat("Near plane distance", &m_cameraConfig.m_near , 0.f, 10.0f, "near = %.3f");
		m_isDirty |= ImGui::SliderFloat("Far plane distance", &m_cameraConfig.m_far, 0.f, 1000.0f, "far = %.3f");
		ImGui::Text("1/far = %f", 1.f / m_cameraConfig.m_far);
		m_isDirty |= ImGui::SliderFloat("Exposure", &m_cameraConfig.m_exposure, 0, 10.0f, "exposure = %.3f");


		auto ShowMat4x4 = [](char const* label, glm::mat4x4 const& matrix)
		{
			if (ImGui::TreeNode(label))
			{
				if (ImGui::BeginTable("table1", 4))
				{
					for (int row = 0; row < 4; row++)
					{
						ImGui::TableNextRow();
						for (int column = 0; column < 4; column++)
						{
							ImGui::TableSetColumnIndex(column);
							ImGui::Text("%f", matrix[row][column]);
						}
					}
					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
		};


		ShowMat4x4("View Matrix:", GetViewMatrix());

		const glm::mat4x4 invView = GetInverseViewMatrix();
		ShowMat4x4("Inverse View Matrix:", invView);

		ShowMat4x4("Projection Matrix:", m_projection);

		const glm::mat4x4 invProj = GetInverseProjectionMatrix();
		ShowMat4x4("Inverse Projection Matrix:", invProj);		

		const glm::mat4x4 viewProj = GetViewProjectionMatrix();
		ShowMat4x4("View Projection Matrix:", viewProj);

		const glm::mat4x4 invViewProj = GetInverseViewProjectionMatrix();
		ShowMat4x4("Inverse View Projection Matrix:", invViewProj);
	}
}

