#include <glm/glm.hpp>

#include "ImageBasedLight.h"
#include "CoreEngine.h"
#include "Texture.h"
#include "BuildConfiguration.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"


namespace SaberEngine
{
	ImageBasedLight::ImageBasedLight(string lightName, string relativeHDRPath) 
		: Light(lightName, LIGHT_AMBIENT_IBL, vec3(0))
	{
		// Irradiance Environment Map (IEM) setup:
		m_IEM_Material = 
			std::make_shared<Material>("IEM_Material", std::shared_ptr<Shader>(nullptr), CUBE_MAP_NUM_FACES, true);

		std::shared_ptr<gr::Texture> IEM_Textures = (std::shared_ptr<gr::Texture>)ConvertEquirectangularToCubemap(
				CoreEngine::GetSceneManager()->GetCurrentSceneName(), 
				relativeHDRPath, 
				m_xRes,
				m_yRes, 
				IBL_IEM);

		if (IEM_Textures != nullptr)
		{
			m_IEM_isValid = true;

			m_IEM_Material->AttachCubeMapTextures(IEM_Textures);
		}

		// Pre-filtered Mipmaped Radiance Environment Map (PMREM) setup:
		m_PMREM_Material = 
			std::make_shared<Material>("PMREM_Material", std::shared_ptr<Shader>(nullptr), CUBE_MAP_NUM_FACES, true);

		std::shared_ptr<gr::Texture> PMREM_Textures = (std::shared_ptr<gr::Texture>)ConvertEquirectangularToCubemap(
			CoreEngine::GetSceneManager()->GetCurrentSceneName(),
			relativeHDRPath, 
			m_xRes, 
			m_yRes,
			IBL_PMREM);

		if (PMREM_Textures != nullptr)
		{
			m_PMREM_isValid = true;

			m_PMREM_Material->AttachCubeMapTextures(PMREM_Textures);

			// Note: We assume the cubemap is always square and use xRes only during our calculations...
			m_maxMipLevel = (uint32_t)glm::log2((float)m_xRes);
		}

		// Render BRDF Integration map:
		GenerateBRDFIntegrationMap();

		// Upload shader parameters:
		if (DeferredMaterial() != nullptr && DeferredMaterial()->GetShader() != nullptr)
		{
			DeferredMaterial()->GetShader()->UploadUniform("maxMipLevel", &m_maxMipLevel, UNIFORM_Int);
		}
		else
		{
			LOG_ERROR("ImageBasedLight could not upload shader parameters");
		}
	}

	
	ImageBasedLight::~ImageBasedLight()
	{
		m_IEM_Material = nullptr;
		m_IEM_isValid = false;

		m_PMREM_Material = nullptr;
		m_PMREM_isValid = false;

		m_BRDF_integrationMap = nullptr;
	}


	std::shared_ptr<gr::Texture> ImageBasedLight::ConvertEquirectangularToCubemap(
		string sceneName, 
		string relativeHDRPath,
		int xRes,
		int yRes, 
		IBL_TYPE iblType /*= RAW_HDR*/)
	{
		string cubemapName;
		vector<string> shaderKeywords;
		uint32_t textureUnit = -1;	// For now, derrive the cube map texture unit based on the type of texture
		if (iblType == IBL_IEM)
		{
			cubemapName = "IBL_IEM";
			shaderKeywords.push_back("BLIT_IEM");
			textureUnit = (uint32_t)CUBE_MAP_0;
		}
		else if (iblType == IBL_PMREM)
		{
			cubemapName = "IBL_PMREM";
			shaderKeywords.push_back("BLIT_PMREM");
			textureUnit = (uint32_t)CUBE_MAP_1;
		}
		else
		{
			cubemapName = "HDR_Image";
			textureUnit = (uint32_t)CUBE_MAP_0;
			// No need to insert any shader keywords
		}

		// Create our conversion shader:
		string shaderName = 
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("equilinearToCubemapBlitShaderName");

		std::shared_ptr<Shader> equirectangularToCubemapBlitShader	= Shader::CreateShader(shaderName, &shaderKeywords);
		if (equirectangularToCubemapBlitShader == nullptr)
		{
			LOG_ERROR("Failed to load equilinearToCubemapBlitShader, cannot convert HDR image to cubemap");
			return nullptr;
		}
		equirectangularToCubemapBlitShader->Bind(true);


		// Create a cube mesh for rendering:
		std::shared_ptr<gr::Mesh> cubeMesh = gr::meshfactory::CreateCube();
		cubeMesh->Bind(true);


		// Load the HDR image:
		string iblTexturePath = 
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + sceneName + "\\" + relativeHDRPath;
		
		std::shared_ptr<gr::Texture> hdrTexture	= CoreEngine::GetSceneManager()->FindLoadTextureByPath(
			iblTexturePath, gr::Texture::TextureColorSpace::Linear); // Deallocated by SceneManager

		if (hdrTexture == nullptr)
		{
			LOG_ERROR("Failed to load HDR texture \"" + iblTexturePath + "\" for image-based lighting");

			// Cleanup:
			equirectangularToCubemapBlitShader = nullptr;

			return nullptr;
		}
		gr::Texture::TextureParams hdrParams = hdrTexture->GetTextureParams();
		hdrParams.m_texSamplerMode = gr::Texture::TextureSamplerMode::Clamp;
		hdrParams.m_texMinMode = gr::Texture::TextureMinFilter::LinearMipMapLinear;
		hdrParams.m_texMaxMode = gr::Texture::TextureMaxFilter::Linear;
		hdrTexture->SetTextureParams(hdrParams);
		
		hdrTexture->Bind(TEXTURE_0 + TEXTURE_ALBEDO, true);


		// TODO: We should just sample spherical textures directly instead of rendering them into cubemaps...


		// Set shader parameters:
		//-----------------------
		int numSamples = 1;
		if (iblType == IBL_IEM)
		{
			numSamples = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("numIEMSamples");
		}
		else if (iblType == IBL_PMREM)
		{
			numSamples = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("numPMREMSamples");
		}
		// "numSamples" is defined directly in equilinearToCubemapBlitShader.frag
		equirectangularToCubemapBlitShader->UploadUniform("numSamples", &numSamples, UNIFORM_Int); 

		// Upload the texel size for the hdr texture:
		vec4 texelSize = hdrTexture->GetTexelDimenions();
		equirectangularToCubemapBlitShader->UploadUniform("texelSize", &texelSize.x, UNIFORM_Vec4fv);

		// Create and upload projection matrix:
		glm::mat4 m_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		equirectangularToCubemapBlitShader->UploadUniform("in_projection", &m_projection[0][0], UNIFORM_Matrix4fv);

		// Create view matrices: Orient the camera towards each face of the cube
		glm::mat4 captureViews[] =
		{
			// TODO: Move this to a common factory somewhere
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};


		// Create a cubemap to render the IBL into:
		gr::Texture::TextureParams cubeParams;
		cubeParams.m_width = xRes;
		cubeParams.m_height = yRes;
		cubeParams.m_faces = SaberEngine::TEXTURE_TYPE::CUBE_MAP_NUM_FACES;
		cubeParams.m_texUse = gr::Texture::TextureUse::ColorTarget;
		cubeParams.m_texDimension = gr::Texture::TextureDimension::TextureCubeMap;
		cubeParams.m_texFormat = gr::Texture::TextureFormat::RGB16F;
		cubeParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;
		cubeParams.m_texSamplerMode = gr::Texture::TextureSamplerMode::Wrap;
		cubeParams.m_texMinMode = iblType == IBL_PMREM ?
			gr::Texture::TextureMinFilter::LinearMipMapLinear :
			gr::Texture::TextureMinFilter::Linear;
		cubeParams.m_texMaxMode = gr::Texture::TextureMaxFilter::Linear;
		cubeParams.m_texturePath = cubemapName;

		// Generate mip-maps for PMREM IBL cubemap faces, to ensure they're allocated once we want to write into them:
		cubeParams.m_useMIPs = iblType == IBL_PMREM ? true : false;

		std::shared_ptr<gr::Texture> cubemap = std::make_shared<gr::Texture>(cubeParams);
		
		// Target set initialization:
		gr::TextureTargetSet m_IBL_IEM_PMREM_StageTargetSet;
		m_IBL_IEM_PMREM_StageTargetSet.ColorTarget(0) = cubemap;
		m_IBL_IEM_PMREM_StageTargetSet.Viewport() = gr::Viewport(0, 0, xRes, yRes);
		m_IBL_IEM_PMREM_StageTargetSet.CreateColorTargets(textureUnit);


		// Render into the cube map:
		//--------------------------
		glDepthFunc(GL_LEQUAL);			// Ensure we can render on the far plane
		
		// Disable back-face culling, since we're rendering a cube from the inside		
		CoreEngine::GetRenderManager()->GetContext().SetCullingMode(platform::Context::FaceCullingMode::Disabled);

		// Handle per-mip-map rendering:
		if (iblType == IBL_PMREM)
		{
			// Calculate the number of mip levels we need to render:
			const uint32_t numMipLevels = cubemap->GetNumMips();

			for (uint32_t currentMipLevel = 0; currentMipLevel < numMipLevels; currentMipLevel++)
			{
				// Compute the roughness for the current mip level, and upload it to the shader:
				float roughness = (float)currentMipLevel / (float)(numMipLevels - 1);
				equirectangularToCubemapBlitShader->UploadUniform("roughness", &roughness, UNIFORM_Float);

				// Render each cube face:
				for (uint32_t face = 0; face < CUBE_MAP_NUM_FACES; ++face)
				{
					equirectangularToCubemapBlitShader->UploadUniform(
						"in_view", 
						&captureViews[face][0].x, 
						UNIFORM_Matrix4fv);

					m_IBL_IEM_PMREM_StageTargetSet.AttachColorTargets(face, currentMipLevel, true);

					CoreEngine::GetRenderManager()->GetContext().ClearTargets(platform::Context::ClearTarget::ColorDepth);

					glDrawElements(GL_TRIANGLES,				// GLenum mode
						(GLsizei)cubeMesh->NumIndices(),		// GLsizei count
						GL_UNSIGNED_INT,						// GLenum type
						(void*)(0));							// const GLvoid* indices
				}
			}
		}
		else // IBL_IEM + RAW_HDR: Non-mip-mapped cube faces
		{
			// Render each cube face:
			for (uint32_t face = 0; face < CUBE_MAP_NUM_FACES; face++)
			{
				equirectangularToCubemapBlitShader->UploadUniform(
					"in_view", 
					&captureViews[face][0].x,
					UNIFORM_Matrix4fv);

				m_IBL_IEM_PMREM_StageTargetSet.AttachColorTargets(face, 0, true);
												
				CoreEngine::GetRenderManager()->GetContext().ClearTargets(platform::Context::ClearTarget::ColorDepth);

				glDrawElements(GL_TRIANGLES,			// GLenum mode
					(GLsizei)cubeMesh->NumIndices(),	// GLsizei count
					GL_UNSIGNED_INT,					// GLenum type
					(void*)(0));						// const GLvoid* indices
			}
		}
		
		// Cleanup:
		m_IBL_IEM_PMREM_StageTargetSet.AttachColorTargets(0, 0, false);

		// Restore defaults:
		glDepthFunc(GL_LESS);
		CoreEngine::GetRenderManager()->GetContext().SetCullingMode(platform::Context::FaceCullingMode::Back);
		
		// Unbind: Texture will be destroyed/deleted by the SceneManager
		hdrTexture->Bind(TEXTURE_0 + TEXTURE_ALBEDO, false);

		cubeMesh->Bind(false);
		cubeMesh = nullptr;

		equirectangularToCubemapBlitShader->Bind(false);
		equirectangularToCubemapBlitShader = nullptr;

		return cubemap;
	}


	void ImageBasedLight::GenerateBRDFIntegrationMap()
	{
		LOG("Rendering BRDF Integration map texture");		
		
		// Destroy any existing map
		if (m_BRDF_integrationMap != nullptr)
		{
			m_BRDF_integrationMap = nullptr;
		}
		
		// Create a shader:
		string shaderName = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("BRDFIntegrationMapShaderName");
		std::shared_ptr<Shader> BRDFIntegrationMapShader = Shader::CreateShader(shaderName);

		if (BRDFIntegrationMapShader == nullptr)
		{
			LOG_ERROR("Failed to load \"" + shaderName + "\", BRDF Integration map generation failed.");
			return;
		}
		BRDFIntegrationMapShader->Bind(true);


		// Create a CCW screen-aligned quad to render with:
		std::shared_ptr<gr::Mesh> quad = gr::meshfactory::CreateQuad
		(
			// TODO: SIMPLIFY THIS INTERFACE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			vec3(-1.0f, 1.0f, -1.0f),	// TL
			vec3(1.0f, 1.0f, -1.0f),	// TR
			vec3(-1.0f, -1.0f, -1.0f),	// BL
			vec3(1.0f, -1.0f, -1.0f)	// BR
		);
		quad->Bind(true);


		// Render into the quad:
		//----------------------

		// Create a render texture:
		gr::Texture::TextureParams brdfParams;
		brdfParams.m_width = m_xRes;
		brdfParams.m_height = m_yRes;
		brdfParams.m_faces = 1;
		brdfParams.m_texUse = gr::Texture::TextureUse::ColorTarget;
		brdfParams.m_texDimension = gr::Texture::TextureDimension::Texture2D;
		brdfParams.m_texFormat = gr::Texture::TextureFormat::RG16F; // 2 channel, 16-bit floating point precision, as recommended by Epic Games:
		brdfParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;
		brdfParams.m_texSamplerMode = gr::Texture::TextureSamplerMode::Clamp;
		brdfParams.m_texMinMode = gr::Texture::TextureMinFilter::Linear;
		brdfParams.m_texMaxMode = gr::Texture::TextureMaxFilter::Linear;
		brdfParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		brdfParams.m_texturePath = "BRDFIntegrationMap";

		m_BRDF_integrationMap = std::make_shared<gr::Texture>(brdfParams);
	
		// Configure a TextureTargetSet:
		m_BRDF_integrationMapStageTargetSet.ColorTarget(0) = gr::TextureTarget(m_BRDF_integrationMap);
		m_BRDF_integrationMapStageTargetSet.Viewport() = gr::Viewport(0, 0, m_xRes, m_yRes);


		// TODO: Bind function should call CreateColorTargets internally
		m_BRDF_integrationMapStageTargetSet.CreateColorTargets(GENERIC_TEXTURE_0);

		// Note: Need to bind so the viewport gets set (even though everything still bound after CreateColorTargets())
		m_BRDF_integrationMapStageTargetSet.AttachColorTargets(0, 0, true);


		// TODO: Handle depth/clearing config via stage params: Stages should control how they interact with the targets
		glDepthFunc(GL_LEQUAL);	// Ensure we can render on the far plane
		CoreEngine::GetRenderManager()->GetContext().ClearTargets(platform::Context::ClearTarget::ColorDepth);

		// Draw:		
		glDrawElements(GL_TRIANGLES,
			(GLsizei)quad->NumIndices(),
			GL_UNSIGNED_INT, 
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);


		// Cleanup:
		m_BRDF_integrationMapStageTargetSet.AttachColorTargets(0, 0, false);
		quad->Bind(false);

		BRDFIntegrationMapShader->Bind(false);
		BRDFIntegrationMapShader = nullptr;
	}
}