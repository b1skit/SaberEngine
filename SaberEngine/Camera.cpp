#include <glm/glm.hpp>

#include "BuildConfiguration.h"
#include "Camera.h"
#include "CoreEngine.h"
#include "Texture.h"
#include "Material.h"
#include "Shader.h"
using gr::Material;
using gr::Texture;
using gr::Shader;
using std::shared_ptr;


namespace SaberEngine
{
	// Default constructor
	Camera::Camera(string cameraName) : SceneObject::SceneObject(cameraName)
	{
		Initialize();	// Initialize with default values

		/*isDirty = false;*/
	}


	Camera::Camera(string cameraName, CameraConfig camConfig, Transform* parent /*= nullptr*/) : SceneObject::SceneObject(cameraName)
	{
		m_cameraConfig = camConfig;

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

		View();						// Internally initializes the view matrix

		m_viewProjection = m_projection * m_view;
	}


	void Camera::Destroy()
	{
		m_cameraShader = nullptr;
	}


	mat4 const& Camera::View()
	{
		m_view = inverse(m_transform.Model());
		return m_view;
	}


	mat4 const* Camera::CubeView()
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

		return &m_cubeView[0];
	}

	mat4 const* Camera::CubeViewProjection()
	{
		// If we've never allocated cubeViewProjection, do it now:
		if (m_cubeViewProjection.size() == 0)
		{
			m_cubeViewProjection.reserve(6);

			mat4 const* ourCubeViews = CubeView(); // Call this to ensure cubeView has been initialized

			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[0]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[1]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[2]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[3]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[4]);
			m_cubeViewProjection.emplace_back(m_projection * ourCubeViews[5]);
		}

		return &m_cubeViewProjection[0];
	}


	void Camera::AttachGBuffer()
	{
		m_cameraShader = std::make_shared<gr::Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("gBufferFillShaderName"));
		m_cameraShader->Create();

		// Create GBuffer color targets:
		gr::Texture::TextureParams gBufferParams;
		gBufferParams.m_width = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		gBufferParams.m_height = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");
		gBufferParams.m_faces = 1;
		gBufferParams.m_texUse = gr::Texture::TextureUse::ColorTarget;
		gBufferParams.m_texDimension = gr::Texture::TextureDimension::Texture2D;
		gBufferParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F; // Using 4 channels for future flexibility
		gBufferParams.m_texColorSpace = gr::Texture::TextureColorSpace::sRGB;
		gBufferParams.m_texSamplerMode = gr::Texture::TextureSamplerMode::Wrap;
		gBufferParams.m_texMinMode = gr::Texture::TextureMinFilter::Linear; // Black output w/NearestMipMapLinear?
		gBufferParams.m_texMaxMode = gr::Texture::TextureMaxFilter::Linear;
		gBufferParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		for (size_t i = 0; i < (size_t)Material::GBuffer_Count; i++)
		{
			if ((Material::TextureSlot)i == Material::GBufferDepth)
			{
				continue;
			}

			std::shared_ptr<gr::Texture> gBufferTex = std::make_shared<gr::Texture>(gBufferParams);

			gBufferTex->SetTexturePath(
				GetName() + "_" + Material::k_GBufferTexNames[(Material::TextureSlot)i]);

			m_camTargetSet.ColorTarget(i) = gBufferTex;
		}

		// Create GBuffer depth target:
		gr::Texture::TextureParams depthTexParams(gBufferParams);
		depthTexParams.m_texUse = gr::Texture::TextureUse::DepthTarget;
		depthTexParams.m_texFormat = gr::Texture::TextureFormat::Depth32F;
		depthTexParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;
		depthTexParams.m_texSamplerMode = gr::Texture::TextureSamplerMode::Clamp;

		std::shared_ptr<gr::Texture> depthTex = std::make_shared<gr::Texture>(depthTexParams);

		depthTex->SetTexturePath(GetName() + "_" + Material::k_GBufferTexNames[Material::GBufferDepth]);

		m_camTargetSet.DepthStencilTarget() = depthTex;

		// Finally, initialize the target set:
		m_camTargetSet.CreateColorDepthStencilTargets(Material::GBufferAlbedo, Material::GBufferDepth);
	}


	void Camera::DebugPrint()
	{
		#if defined(DEBUG_TRANSFORMS)
			LOG("\n[CAMERA DEBUG: " + GetName() + "]");
			LOG("Field of view: " + to_string(m_fieldOfView));
			LOG("Near: " + to_string(m_near));
			LOG("Far: " + to_string(m_far));
			LOG("Aspect ratio: " + to_string(m_aspectRatio));

			LOG("Position: " + to_string(m_transform.WorldPosition().x) + " " + to_string(m_transform.WorldPosition().y) + " " + to_string(m_transform.WorldPosition().z));
			LOG("Euler Rotation: " + to_string(m_transform.GetEulerRotation().x) + " " + to_string(m_transform.GetEulerRotation().y) + " " + to_string(m_transform.GetEulerRotation().z));
			
			// NOTE: OpenGL matrics are stored in column-major order
			LOG("\nView Matrix:\n\t" + to_string(m_view[0][0]) + " " + to_string(m_view[1][0]) + " " + to_string(m_view[2][0]) + " " + to_string(m_view[3][0]) );
			LOG("\t" + to_string(m_view[0][1]) + " " + to_string(m_view[1][1]) + " " + to_string(m_view[2][1]) + " " + to_string(m_view[3][1]));
			LOG("\t" + to_string(m_view[0][2]) + " " + to_string(m_view[1][2]) + " " + to_string(m_view[2][2]) + " " + to_string(m_view[3][2]));
			LOG("\t" + to_string(m_view[0][3]) + " " + to_string(m_view[1][3]) + " " + to_string(m_view[2][3]) + " " + to_string(m_view[3][3]));

			LOG("\nProjection Matrix:\n\t" + to_string(m_projection[0][0]) + " " + to_string(m_projection[1][0]) + " " + to_string(m_projection[2][0]) + " " + to_string(m_projection[3][0]));
			LOG("\t" + to_string(m_projection[0][1]) + " " + to_string(m_projection[1][1]) + " " + to_string(m_projection[2][1]) + " " + to_string(m_projection[3][1]));
			LOG("\t" + to_string(m_projection[0][2]) + " " + to_string(m_projection[1][2]) + " " + to_string(m_projection[2][2]) + " " + to_string(m_projection[3][2]));
			LOG("\t" + to_string(m_projection[0][3]) + " " + to_string(m_projection[1][3]) + " " + to_string(m_projection[2][3]) + " " + to_string(m_projection[3][3]));

			LOG("\nView Projection Matrix:\n\t" + to_string(m_viewProjection[0][0]) + " " + to_string(m_viewProjection[1][0]) + " " + to_string(m_viewProjection[2][0]) + " " + to_string(m_viewProjection[3][0]));
			LOG("\t" + to_string(m_viewProjection[0][1]) + " " + to_string(m_viewProjection[1][1]) + " " + to_string(m_viewProjection[2][1]) + " " + to_string(m_viewProjection[3][1]));
			LOG("\t" + to_string(m_viewProjection[0][2]) + " " + to_string(m_viewProjection[1][2]) + " " + to_string(m_viewProjection[2][2]) + " " + to_string(m_viewProjection[3][2]));
			LOG("\t" + to_string(m_viewProjection[0][3]) + " " + to_string(m_viewProjection[1][3]) + " " + to_string(m_viewProjection[2][3]) + " " + to_string(m_viewProjection[3][3]));
		#else
			return;
		#endif
	}
}

