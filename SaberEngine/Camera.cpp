#include <glm/glm.hpp>

#include "DebugConfiguration.h"
#include "Camera.h"
#include "CoreEngine.h"
#include "Texture.h"
#include "Material.h"
#include "Shader.h"

using gr::Material;
using gr::Texture;
using gr::Shader;
using gr::Transform;
using std::shared_ptr;
using glm::mat4;

namespace gr
{
	Camera::Camera(string cameraName, CameraConfig camConfig, Transform* parent) : SceneObject::SceneObject(cameraName), 
		m_cameraConfig(camConfig),
		m_cameraShader(nullptr)
	{
		m_transform.SetParent(parent);

		Initialize();
	}


	void Camera::Initialize()
	{
		if (m_cameraConfig.m_isOrthographic)
		{
			m_cameraConfig.m_fieldOfView = 0.0f;

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
				glm::radians(m_cameraConfig.m_fieldOfView), 
				m_cameraConfig.m_aspectRatio, 
				m_cameraConfig.m_near, 
				m_cameraConfig.m_far
			);
		}		
	}


	void Camera::Destroy()
	{
		m_cameraShader = nullptr;
	}


	mat4 Camera::GetViewMatrix() const
	{
		return inverse(m_transform.Model());
	}


	std::vector<glm::mat4> const& Camera::GetCubeViewMatrix()
	{
		// If we've never allocated cubeView, do it now:
		if (m_cubeView.size() == 0)
		{
			m_cubeView.reserve(6);

			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.GetWorldPosition(), 
				m_transform.GetWorldPosition() + Transform::WORLD_X,
				-Transform::WORLD_Y) );
			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.GetWorldPosition(), 
				m_transform.GetWorldPosition() - Transform::WORLD_X, 
				-Transform::WORLD_Y) );

			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.GetWorldPosition(), 
				m_transform.GetWorldPosition() + Transform::WORLD_Y, 
					Transform::WORLD_Z) );
			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.GetWorldPosition(), 
				m_transform.GetWorldPosition() - Transform::WORLD_Y, 
				-Transform::WORLD_Z) );

			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.GetWorldPosition(), 
				m_transform.GetWorldPosition() + Transform::WORLD_Z,
				-Transform::WORLD_Y) );
			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.GetWorldPosition(), 
				m_transform.GetWorldPosition() - Transform::WORLD_Z, 
				-Transform::WORLD_Y) );
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
}

