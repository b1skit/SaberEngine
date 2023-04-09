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
	{
		m_cubeView.reserve(6);
		Initialize();
	}


	void Camera::Update(const double stepTimeMs)
	{
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
		m_cameraPBData.g_projectionParams = 
			glm::vec4(1.f, m_cameraConfig.m_near, m_cameraConfig.m_far, 1.0f / m_cameraConfig.m_far);

		m_cameraPBData.g_cameraWPos = GetTransform()->GetGlobalPosition();

		m_cameraParamBlock->Commit(m_cameraPBData);

		// TODO: It's possible to update the camera params multiple times in a frame if SetCameraConfig is called by
		// another object in the Updateable list.
		// eg. Light::Update -> SetCameraConfig
		// -> Need to switch to a scene graph representation so the update order each frame is determinate
	}


	void Camera::Initialize()
	{
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
				"CameraParams",
				m_cameraPBData, // Initialize with a default struct: Updated in UpdateCameraParamBlockData()
				re::ParameterBlock::PBType::Mutable);
		}

		UpdateCameraParamBlockData();
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
		Initialize();
	}
}

