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
using std::shared_ptr;


namespace gr
{
	Camera::Camera(string cameraName, CameraConfig camConfig, Transform* parent) :
		SceneObject::SceneObject(cameraName), 
		m_cameraConfig(camConfig),
		m_cameraShader(nullptr)
	{
		m_transform.Parent(parent);

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

		m_viewProjection = m_projection * GetViewMatrix(); // Internally initializes the view matrix
	}


	void Camera::Destroy()
	{
		m_cameraShader = nullptr;
	}


	mat4 const& Camera::GetViewMatrix()
	{
		m_view = inverse(m_transform.Model());
		return m_view;
	}


	mat4 const& Camera::GetCubeViewMatrix()
	{
		// If we've never allocated cubeView, do it now:
		if (m_cubeView.size() == 0)
		{
			m_cubeView.reserve(6);

			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.WorldPosition(), 
				m_transform.WorldPosition() + Transform::WORLD_X,
				-Transform::WORLD_Y) );
			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.WorldPosition(), 
				m_transform.WorldPosition() - Transform::WORLD_X, 
				-Transform::WORLD_Y) );

			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.WorldPosition(), 
				m_transform.WorldPosition() + Transform::WORLD_Y, 
					Transform::WORLD_Z) );
			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.WorldPosition(), 
				m_transform.WorldPosition() - Transform::WORLD_Y, 
				-Transform::WORLD_Z) );

			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.WorldPosition(), 
				m_transform.WorldPosition() + Transform::WORLD_Z,
				-Transform::WORLD_Y) );
			m_cubeView.emplace_back( 
				glm::lookAt(m_transform.WorldPosition(), 
				m_transform.WorldPosition() - Transform::WORLD_Z, 
				-Transform::WORLD_Y) );
		}

		// TODO: Recalculate this if the camera has moved

		return m_cubeView[0];
	}

	mat4 const& Camera::GetCubeViewProjectionMatrix()
	{
		// If we've never allocated cubeViewProjection, do it now:
		if (m_cubeViewProjection.size() == 0)
		{
			m_cubeViewProjection.reserve(6);

			mat4 const& ourCubeViews = GetCubeViewMatrix(); // Call this to ensure cubeView has been initialized

			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews);
		}

		return m_cubeViewProjection[0];
	}


	void Camera::AttachGBuffer()
	{
		GetName() = "GBufferCam";

		const vector<string> gBufferTexNames
		{
			"GBufferAlbedo",	// 0
			"GBufferWNormal",	// 1
			"GBufferRMAO",		// 2
			"GBufferEmissive",	// 3
			"GBufferWPos",		// 4
			"GBufferMatProp0",	// 5
			"GBufferDepth",		// 6
		};

		m_cameraShader = std::make_shared<gr::Shader>(
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("gBufferFillShaderName"));
		m_cameraShader->Create();

		// Create GBuffer color targets:
		gr::Texture::TextureParams gBufferParams;
		gBufferParams.m_width = SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		gBufferParams.m_height = SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");
		gBufferParams.m_faces = 1;
		gBufferParams.m_texUse = gr::Texture::TextureUse::ColorTarget;
		gBufferParams.m_texDimension = gr::Texture::TextureDimension::Texture2D;
		gBufferParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F; // Using 4 channels for future flexibility
		gBufferParams.m_texColorSpace = gr::Texture::TextureColorSpace::sRGB;
		gBufferParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);


		gBufferParams.m_useMIPs = false;
		// TODO: Currently, our GBuffer doesn't use mipmapping, but it should.
		// We need to compute the appropriate mip level in the shader, by writing UV derivatives during the GBuffer
		// pass, and using a stencil mask to ensure we're sampling the correct material at boundaries
		// https://www.reedbeta.com/blog/deferred-texturing/
		// -> We'll also need to trigger mip generation after laying down the GBuffer


		for (size_t i = 0; i <= 5; i++)
		{
			std::shared_ptr<gr::Texture> gBufferTex = std::make_shared<gr::Texture>(gBufferParams);

			gBufferTex->SetTexturePath(GetName() + "_" + gBufferTexNames[i]);

			m_camTargetSet.ColorTarget(i) = gBufferTex;
		}

		// Create GBuffer depth target:
		gr::Texture::TextureParams depthTexParams(gBufferParams);
		depthTexParams.m_texUse = gr::Texture::TextureUse::DepthTarget;
		depthTexParams.m_texFormat = gr::Texture::TextureFormat::Depth32F;
		depthTexParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;

		std::shared_ptr<gr::Texture> depthTex = std::make_shared<gr::Texture>(depthTexParams);

		depthTex->SetTexturePath(GetName() + "_" + gBufferTexNames[Material::GBufferDepth]);

		m_camTargetSet.DepthStencilTarget() = depthTex;

		// Finally, initialize the target set:
		m_camTargetSet.CreateColorDepthStencilTargets(Material::GBufferAlbedo, Material::GBufferDepth);
	}
}

