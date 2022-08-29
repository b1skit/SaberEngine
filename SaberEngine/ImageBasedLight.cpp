#include <glm/glm.hpp>

#include "ImageBasedLight.h"
#include "CoreEngine.h"
#include "Texture.h"
#include "DebugConfiguration.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
using gr::Material;
using gr::Texture;
using gr::Shader;
using std::shared_ptr;
using std::make_shared;


namespace SaberEngine
{
	ImageBasedLight::ImageBasedLight(string lightName, string relativeHDRPath) 
		: Light(lightName, AmbientIBL, vec3(0))
	{
		// Irradiance Environment Map (IEM) setup:

		m_IEM_Tex = ConvertEquirectangularToCubemap(
				CoreEngine::GetSceneManager()->GetCurrentSceneName(), 
				relativeHDRPath, 
				m_xRes,
				m_yRes, 
				IBL_IEM);

		if (m_IEM_Tex != nullptr)
		{
			m_IEM_isValid = true;
		}

		// Pre-filtered Mipmaped Radiance Environment Map (PMREM) setup:

		m_PMREM_Tex = ConvertEquirectangularToCubemap(
			CoreEngine::GetSceneManager()->GetCurrentSceneName(),
			relativeHDRPath, 
			m_xRes, 
			m_yRes,
			IBL_PMREM);

		if (m_PMREM_Tex != nullptr)
		{
			m_PMREM_isValid = true;

			// Note: We assume the cubemap is always square and use xRes only during our calculations...
			m_maxMipLevel = (uint32_t)glm::log2((float)m_xRes);
		}

		// Render BRDF Integration map:
		GenerateBRDFIntegrationMap();

		// Upload shader parameters:
		if (GetDeferredLightShader() != nullptr)
		{
			GetDeferredLightShader()->SetUniform("maxMipLevel", &m_maxMipLevel, platform::Shader::UNIFORM_TYPE::Int);
		}
		else
		{
			LOG_ERROR("ImageBasedLight could not upload shader parameters");
		}
	}

	
	ImageBasedLight::~ImageBasedLight()
	{
		m_IEM_Tex = nullptr;
		m_IEM_isValid = false;

		m_PMREM_Tex = nullptr;
		m_PMREM_isValid = false;

		m_BRDF_integrationMap = nullptr;
	}


	shared_ptr<gr::Texture> ImageBasedLight::ConvertEquirectangularToCubemap(
		string sceneName, 
		string relativeHDRPath,
		int xRes,
		int yRes, 
		IBL_TYPE iblType /*= RAW_HDR*/)
	{
		// Create our conversion shader:
		string shaderName =
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("equilinearToCubemapBlitShaderName");

		shared_ptr<Shader> equirectangularToCubemapBlitShader = make_shared<gr::Shader>(shaderName);

		string cubemapName;
		uint32_t textureUnit = -1;	// For now, derrive the cube map texture unit based on the type of texture
		if (iblType == IBL_IEM)
		{
			cubemapName = "IBL_IEM";
			equirectangularToCubemapBlitShader->ShaderKeywords().emplace_back("BLIT_IEM");
			textureUnit = (uint32_t)Material::CubeMap0;
		}
		else if (iblType == IBL_PMREM)
		{
			cubemapName = "IBL_PMREM";
			equirectangularToCubemapBlitShader->ShaderKeywords().emplace_back("BLIT_PMREM");
			textureUnit = (uint32_t)Material::CubeMap1;
		}
		else
		{
			cubemapName = "HDR_Image";
			textureUnit = (uint32_t)Material::CubeMap0;
			// No need to insert any shader keywords
		}

		equirectangularToCubemapBlitShader->Create();
		equirectangularToCubemapBlitShader->Bind(true);

		// Create a cube mesh for rendering:
		shared_ptr<gr::Mesh> cubeMesh = gr::meshfactory::CreateCube();
		cubeMesh->Bind(true);


		// Load the HDR image:
		const string iblTexturePath = 
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + sceneName + "\\" + relativeHDRPath;
		
		shared_ptr<gr::Texture> hdrTexture	= CoreEngine::GetSceneManager()->FindLoadTextureByPath(
			iblTexturePath, Texture::TextureColorSpace::Linear); // Deallocated by SceneManager

		if (hdrTexture == nullptr)
		{
			LOG_ERROR("Failed to load HDR texture \"" + iblTexturePath + "\" for image-based lighting");

			return nullptr;
		}
		
		equirectangularToCubemapBlitShader->SetTexture(
			"MatAlbedo", hdrTexture, gr::Sampler::GetSampler(gr::Sampler::SamplerType::ClampLinearMipMapLinearLinear));



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
		equirectangularToCubemapBlitShader->SetUniform("numSamples", &numSamples, platform::Shader::UNIFORM_TYPE::Int);

		// Upload the texel size for the hdr texture:
		vec4 texelSize = hdrTexture->GetTexelDimenions();
		equirectangularToCubemapBlitShader->SetUniform("texelSize", &texelSize.x, platform::Shader::UNIFORM_TYPE::Vec4f);

		// Create and upload projection matrix:
		glm::mat4 m_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		equirectangularToCubemapBlitShader->SetUniform("in_projection", &m_projection[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);

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
		Texture::TextureParams cubeParams;
		cubeParams.m_width = xRes;
		cubeParams.m_height = yRes;
		cubeParams.m_faces = Texture::k_numCubeFaces;
		cubeParams.m_texUse = Texture::TextureUse::ColorTarget;
		cubeParams.m_texDimension = Texture::TextureDimension::TextureCubeMap;
		cubeParams.m_texFormat = Texture::TextureFormat::RGB16F;
		cubeParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		cubeParams.m_texturePath = cubemapName;

		// Generate mip-maps for PMREM IBL cubemap faces
		cubeParams.m_useMIPs = iblType == IBL_PMREM ? true : false;

		shared_ptr<gr::Texture> cubemap = std::make_shared<gr::Texture>(cubeParams);
		
		// Target set initialization:
		gr::TextureTargetSet m_IBL_IEM_PMREM_StageTargetSet;
		m_IBL_IEM_PMREM_StageTargetSet.ColorTarget(0) = cubemap;
		m_IBL_IEM_PMREM_StageTargetSet.Viewport() = gr::Viewport(0, 0, xRes, yRes);
		m_IBL_IEM_PMREM_StageTargetSet.CreateColorTargets(textureUnit);


		// Render into the cube map:
		//--------------------------
		// Ensure we can render on the far plane
		CoreEngine::GetRenderManager()->GetContext().SetDepthMode(platform::Context::DepthMode::LEqual);

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
				equirectangularToCubemapBlitShader->SetUniform("roughness", &roughness, platform::Shader::UNIFORM_TYPE::Float);

				// Render each cube face:
				for (uint32_t face = 0; face < Texture::k_numCubeFaces; ++face)
				{
					equirectangularToCubemapBlitShader->SetUniform(
						"in_view", 
						&captureViews[face][0].x, 
						platform::Shader::UNIFORM_TYPE::Matrix4x4f);

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
			for (uint32_t face = 0; face < Texture::k_numCubeFaces; face++)
			{
				equirectangularToCubemapBlitShader->SetUniform(
					"in_view", 
					&captureViews[face][0].x,
					platform::Shader::UNIFORM_TYPE::Matrix4x4f);

				m_IBL_IEM_PMREM_StageTargetSet.AttachColorTargets(face, 0, true);
												
				CoreEngine::GetRenderManager()->GetContext().ClearTargets(platform::Context::ClearTarget::ColorDepth);

				glDrawElements(GL_TRIANGLES,			// GLenum mode
					(GLsizei)cubeMesh->NumIndices(),	// GLsizei count
					GL_UNSIGNED_INT,					// GLenum type
					(void*)(0));						// const GLvoid* indices
			}
		}

		// Restore defaults:
		CoreEngine::GetRenderManager()->GetContext().SetDepthMode(platform::Context::DepthMode::Default);
		CoreEngine::GetRenderManager()->GetContext().SetCullingMode(platform::Context::FaceCullingMode::Back);

		return cubemap;
	}


	void ImageBasedLight::GenerateBRDFIntegrationMap()
	{
		LOG("Rendering BRDF Integration map texture");		
		
		// Destroy any existing map
		m_BRDF_integrationMap = nullptr;
		
		// Create a shader:
		const std::string shaderName = 
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("BRDFIntegrationMapShaderName");

		shared_ptr<Shader> BRDFIntegrationMapShader = make_shared<Shader>(shaderName);
		BRDFIntegrationMapShader->Create();
		BRDFIntegrationMapShader->Bind(true);

		// Create a CCW screen-aligned quad to render with:
		shared_ptr<gr::Mesh> quad = gr::meshfactory::CreateQuad
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
		Texture::TextureParams brdfParams;
		brdfParams.m_width = m_xRes;
		brdfParams.m_height = m_yRes;
		brdfParams.m_faces = 1;
		brdfParams.m_texUse = Texture::TextureUse::ColorTarget;
		brdfParams.m_texDimension = Texture::TextureDimension::Texture2D;
		brdfParams.m_texFormat = Texture::TextureFormat::RG16F; // 2 channel, 16-bit floating point precision, as recommended by Epic Games:
		brdfParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		brdfParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		brdfParams.m_texturePath = "BRDFIntegrationMap";

		brdfParams.m_useMIPs = false;

		m_BRDF_integrationMap = std::make_shared<gr::Texture>(brdfParams);
	
		// Configure a TextureTargetSet:
		m_BRDF_integrationMapStageTargetSet.ColorTarget(0) = m_BRDF_integrationMap;
		m_BRDF_integrationMapStageTargetSet.Viewport() = gr::Viewport(0, 0, m_xRes, m_yRes);
		m_BRDF_integrationMapStageTargetSet.CreateColorTargets(Material::Tex7);
		m_BRDF_integrationMapStageTargetSet.AttachColorTargets(0, 0, true);

		// Ensure we can render on the far plane
		CoreEngine::GetRenderManager()->GetContext().SetDepthMode(platform::Context::DepthMode::LEqual);
		CoreEngine::GetRenderManager()->GetContext().ClearTargets(platform::Context::ClearTarget::ColorDepth);
		// TODO: Handle depth/clearing config via stage params: Stages should control how they interact with the targets

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