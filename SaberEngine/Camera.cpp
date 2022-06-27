#include "BuildConfiguration.h"
#include "Camera.h"
#include "CoreEngine.h"
#include "Texture.h"
#include "RenderTexture.h"
#include "Material.h"


#include <glm/glm.hpp>


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
			m_cameraConfig.m_orthoLeft	= 0.0f;
			m_cameraConfig.m_orthoRight	= 0.0f;
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
		if (m_renderMaterial != nullptr)
		{
			for (int i = 0; i < TEXTURE_COUNT; i++)
			{
				Texture* currentTexture = m_renderMaterial->AccessTexture((TEXTURE_TYPE)i);
				if (currentTexture != nullptr)
				{
					currentTexture->Destroy();
					delete currentTexture;
					currentTexture = nullptr;
				}
			}

			delete m_renderMaterial;
			m_renderMaterial = nullptr;
		}
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

			m_cubeView.emplace_back( glm::lookAt(m_transform.WorldPosition(), m_transform.WorldPosition() + Transform::WORLD_X, -Transform::WORLD_Y) );			
			m_cubeView.emplace_back( glm::lookAt(m_transform.WorldPosition(), m_transform.WorldPosition() - Transform::WORLD_X, -Transform::WORLD_Y) );

			m_cubeView.emplace_back( glm::lookAt(m_transform.WorldPosition(), m_transform.WorldPosition() + Transform::WORLD_Y, Transform::WORLD_Z) );
			m_cubeView.emplace_back( glm::lookAt(m_transform.WorldPosition(), m_transform.WorldPosition() - Transform::WORLD_Y, -Transform::WORLD_Z) );

			m_cubeView.emplace_back( glm::lookAt(m_transform.WorldPosition(), m_transform.WorldPosition() + Transform::WORLD_Z, -Transform::WORLD_Y) );
			m_cubeView.emplace_back( glm::lookAt(m_transform.WorldPosition(), m_transform.WorldPosition() - Transform::WORLD_Z, -Transform::WORLD_Y) );
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
		Material* gBufferMaterial	= new Material(GetName() + "_Material", CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("gBufferFillShaderName"), RENDER_TEXTURE_COUNT, true);
		m_renderMaterial		= gBufferMaterial;

		// We use the albedo texture as a basis for the others
		RenderTexture* gBuffer_albedo = new RenderTexture
		(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes"),
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes"),
			GetName() + "_" + Material::RENDER_TEXTURE_SAMPLER_NAMES[RENDER_TEXTURE_ALBEDO]
		);
		gBuffer_albedo->Format()			= GL_RGBA;		// Note: Using 4 channels for future flexibility
		gBuffer_albedo->InternalFormat()	= GL_RGBA32F;		
		
		gBuffer_albedo->TextureMinFilter()	= GL_LINEAR;	// Note: Output is black if this is GL_NEAREST_MIPMAP_LINEAR
		gBuffer_albedo->TextureMaxFilter()	= GL_LINEAR;

		gBuffer_albedo->AttachmentPoint()	= GL_COLOR_ATTACHMENT0 + 0; // Need to increment by 1 each new texture we attach. Used when calling glFramebufferTexture2D()

		gBuffer_albedo->ReadBuffer()		= GL_COLOR_ATTACHMENT0 + 0;
		gBuffer_albedo->DrawBuffer()		= GL_COLOR_ATTACHMENT0 + 0;

		gBuffer_albedo->Buffer(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO);

		GLuint gBufferFBO = gBuffer_albedo->FBO(); // Cache off the FBO for the other GBuffer textures

		gBufferMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO) = gBuffer_albedo;

		// Store references to our additonal RenderTextures:
		int numAdditionalRTs			= (int)RENDER_TEXTURE_COUNT - 2; // -2 b/c we already have 1, and we'll add the depth texture last
		vector<RenderTexture*> additionalRTs(numAdditionalRTs, nullptr);

		int insertIndex				= 0;
		int attachmentIndexOffset	= 1;
		for (int currentType = 1; currentType < (int)RENDER_TEXTURE_COUNT; currentType++)
		{
			if ((TEXTURE_TYPE)currentType == RENDER_TEXTURE_DEPTH)
			{
				continue;
			}

			RenderTexture* currentRT		= new RenderTexture(*gBuffer_albedo);	// We're creating the same type of textures, attached to the same framebuffer
			currentRT->TexturePath()		= GetName() + "_" + Material::RENDER_TEXTURE_SAMPLER_NAMES[(TEXTURE_TYPE)currentType];

			currentRT->FBO()				= gBufferFBO;
			currentRT->AttachmentPoint()	= gBuffer_albedo->AttachmentPoint() + attachmentIndexOffset;
			currentRT->ReadBuffer()			= gBuffer_albedo->AttachmentPoint() + attachmentIndexOffset;
			currentRT->DrawBuffer()			= gBuffer_albedo->AttachmentPoint() + attachmentIndexOffset;
			
			currentRT->Texture::Buffer(RENDER_TEXTURE_0 + currentType);

			currentRT->Bind(RENDER_TEXTURE_0 + currentType, false); // Cleanup: Texture was never unbound in Texture::Buffer, so we must unbind it here

			gBufferMaterial->AccessTexture((TEXTURE_TYPE)currentType)	= currentRT;
			additionalRTs[insertIndex]									= currentRT;

			insertIndex++;
			attachmentIndexOffset++;
		}

		// Attach the textures to the existing FBO
		gBuffer_albedo->AttachAdditionalRenderTexturesToFramebuffer(&additionalRTs[0], numAdditionalRTs);

		// Configure the depth buffer:
		RenderTexture* depth = new RenderTexture
		(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes"),
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes"),
			GetName() + "_" + Material::RENDER_TEXTURE_SAMPLER_NAMES[RENDER_TEXTURE_DEPTH]
		);

		// Add the new texture to our material:
		gBufferMaterial->AccessTexture(RENDER_TEXTURE_DEPTH) = depth;

		depth->TexturePath()	= GetName() + "_" + Material::RENDER_TEXTURE_SAMPLER_NAMES[RENDER_TEXTURE_DEPTH];
		depth->FBO()			= gBufferFBO;

		depth->Texture::Buffer(RENDER_TEXTURE_0 + RENDER_TEXTURE_DEPTH);

		glBindTexture(depth->TextureTarget(), 0); // Cleanup: Texture was never unbound in Texture::Buffer, so we must unbind it here

		gBuffer_albedo->AttachAdditionalRenderTexturesToFramebuffer(&depth, 1, true);
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

