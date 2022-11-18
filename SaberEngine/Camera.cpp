#include <glm/glm.hpp>

#include "DebugConfiguration.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"

using gr::Material;
using gr::Texture;
using gr::Transform;
using std::shared_ptr;
using std::string;
using std::make_shared;
using glm::mat4;


namespace gr
{
	Camera::Camera(string const& cameraName, CameraConfig const& camConfig, Transform* parent) :
		NamedObject(cameraName),
			m_cameraConfig(camConfig)
	{
		m_transform.SetParent(parent);

		// Initialize the param block pointer first:
		m_cameraParamBlock = re::ParameterBlock::Create(
			"CameraParams",
			m_cameraPBData, // Initialize with a default struct: Updated in UpdateCameraParamBlockData()
			re::ParameterBlock::UpdateType::Mutable,
			re::ParameterBlock::Lifetime::Permanent);

		Initialize();
	}


	void Camera::Update(const double stepTimeMs)
	{
		UpdateCameraParamBlockData();
	}


	void Camera::UpdateCameraParamBlockData()
	{
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

		m_cameraParamBlock->SetData(m_cameraPBData);

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
				m_cameraConfig.m_orthoLeft, 
				m_cameraConfig.m_orthoRight, 
				m_cameraConfig.m_orthoBottom, 
				m_cameraConfig.m_orthoTop, 
				m_cameraConfig.m_near, 
				m_cameraConfig.m_far
			);
		}
		else
		{
			m_cameraConfig.m_orthoLeft		= 0.0f;
			m_cameraConfig.m_orthoRight		= 0.0f;
			m_cameraConfig.m_orthoBottom	= 0.0f;
			m_cameraConfig.m_orthoTop		= 0.0f;

			m_projection = glm::perspective
			(
				m_cameraConfig.m_yFOV,
				m_cameraConfig.m_aspectRatio, 
				m_cameraConfig.m_near, 
				m_cameraConfig.m_far
			);
		}

		UpdateCameraParamBlockData();
	}


	void Camera::Destroy()
	{
	}


	std::vector<glm::mat4> const& Camera::GetCubeViewMatrix()
	{
		// If we've never allocated cubeView, do it now:
		if (m_cubeView.size() == 0)
		{
			m_cubeView.reserve(6);

			m_cubeView.emplace_back(glm::lookAt(
				m_transform.GetGlobalPosition(),							// eye
				m_transform.GetGlobalPosition() + Transform::WorldAxisX,	// center: Position the camera is looking at
				-Transform::WorldAxisY));									// Normalized camera up vector
			m_cubeView.emplace_back(glm::lookAt(
				m_transform.GetGlobalPosition(), 
				m_transform.GetGlobalPosition() - Transform::WorldAxisX, 
				-Transform::WorldAxisY));

			m_cubeView.emplace_back(glm::lookAt(
				m_transform.GetGlobalPosition(), 
				m_transform.GetGlobalPosition() + Transform::WorldAxisY, 
					Transform::WorldAxisZ));
			m_cubeView.emplace_back(glm::lookAt(
				m_transform.GetGlobalPosition(), 
				m_transform.GetGlobalPosition() - Transform::WorldAxisY, 
				-Transform::WorldAxisZ));

			m_cubeView.emplace_back(glm::lookAt(
				m_transform.GetGlobalPosition(), 
				m_transform.GetGlobalPosition() + Transform::WorldAxisZ,
				-Transform::WorldAxisY));
			m_cubeView.emplace_back(glm::lookAt(
				m_transform.GetGlobalPosition(), 
				m_transform.GetGlobalPosition() - Transform::WorldAxisZ, 
				-Transform::WorldAxisY));
		}

		// TODO: Recalculate this if the camera has moved

		return m_cubeView;
	}

	std::vector<glm::mat4> const& Camera::GetCubeViewProjectionMatrix()
	{
		// If we've never allocated cubeViewProjection, do it now:
		if (m_cubeViewProjection.size() == 0)
		{
			m_cubeViewProjection.reserve(6);

			// Call this to ensure cubeView has been initialized
			std::vector<glm::mat4> const& ourCubeViews = GetCubeViewMatrix(); 

			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[0]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[1]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[2]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[3]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[4]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[5]);
		}

		return m_cubeViewProjection;
	}


	void Camera::SetCameraConfig(CameraConfig const& newConfig)
	{
		m_cameraConfig = newConfig;
		Initialize();
	}
}

