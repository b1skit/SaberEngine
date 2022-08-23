#include <string>
using std::string;

// TODO: Remove these!!!!!!!!!!!
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;

#include "RenderManager.h"
#include "CoreEngine.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Mesh.h"
#include "Transform.h"
#include "Material.h"
#include "Texture.h"
#include "BuildConfiguration.h"
#include "Skybox.h"
#include "Camera.h"
#include "ImageBasedLight.h"
#include "PostFXManager.h"
#include "ShadowMap.h"
#include "Scene.h"
#include "EventManager.h"
using gr::Material;
using gr::Texture;
using gr::Shader;
using std::shared_ptr;
using std::make_shared;


namespace SaberEngine
{
	RenderManager::~RenderManager()
	{
		// Do this in the destructor so we can still read any final OpenGL error messages before it is destroyed
		m_context.Destroy();
	}


	RenderManager& RenderManager::Instance()
	{
		static RenderManager* instance = new RenderManager();
		return *instance;
	}


	void RenderManager::Startup()
	{
		LOG("RenderManager starting...");

		m_context.Create();

		// Cache the relevant config data:
		m_xRes					= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		m_yRes					= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");

		// Default target set:
		m_defaultTargetSet = std::make_shared<gr::TextureTargetSet>();
		m_defaultTargetSet->Viewport() = { 0, 0, (uint32_t)m_xRes, (uint32_t)m_yRes };
		m_defaultTargetSet->CreateColorTargets(Material::MatAlbedo); // Default framebuffer has no targets

		// Output target:		
		Texture::TextureParams outputParams;
		outputParams.m_width = m_xRes;
		outputParams.m_height = m_yRes;
		outputParams.m_faces = 1;
		outputParams.m_texUse = Texture::TextureUse::ColorTarget;
		outputParams.m_texDimension = Texture::TextureDimension::Texture2D;
		outputParams.m_texFormat = Texture::TextureFormat::RGBA32F;
		outputParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		outputParams.m_texSamplerMode = Texture::TextureSamplerMode::Wrap;
		outputParams.m_texMinMode = Texture::TextureMinFilter::Linear;
		outputParams.m_texMaxMode = Texture::TextureMaxFilter::Linear;
		outputParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		outputParams.m_texturePath = "RenderManagerFrameOutput";

		std::shared_ptr<gr::Texture> outputTexture = std::make_shared<gr::Texture>(outputParams);

		m_outputTargetSet = std::make_shared<gr::TextureTargetSet>();
		m_outputTargetSet->ColorTarget(0) = outputTexture;

		m_outputTargetSet->CreateColorTargets(Material::MatAlbedo);
		
		m_blitShader = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blitShader"));
		m_blitShader->Create();

		// PostFX Manager:
		m_postFXManager = std::make_unique<PostFXManager>(); // Initialized when RenderManager.Initialize() is called

		m_screenAlignedQuad = gr::meshfactory::CreateQuad
		(
			vec3(-1.0f, 1.0f, 0.0f),	// TL
			vec3(1.0f, 1.0f, 0.0f),	// TR
			vec3(-1.0f, -1.0f, 0.0f),	// BL
			vec3(1.0f, -1.0f, 0.0f)	// BR
		);
	}


	void RenderManager::Shutdown()
	{
		LOG("Render manager shutting down...");

		m_blitShader = nullptr;
		m_outputTargetSet = nullptr;
		m_screenAlignedQuad = nullptr;
		m_postFXManager = nullptr;
	}


	void RenderManager::Update()
	{
		std::shared_ptr<Camera> mainCam = CoreEngine::GetSceneManager()->GetCameras(CAMERA_TYPE_MAIN).at(0);

		
		// Disable culling to minimize peter-panning
		m_context.SetCullingMode(platform::Context::FaceCullingMode::Disabled);

		// Fill shadow maps:
		vector<std::shared_ptr<Light>> const& deferredLights = CoreEngine::GetSceneManager()->GetDeferredLights();
		if (!deferredLights.empty())
		{
			for (size_t i = 0; i < (int)deferredLights.size(); i++)
			{
				if (deferredLights.at(i)->GetShadowMap() != nullptr)
				{
					RenderLightShadowMap(deferredLights.at(i));
				}
			}
		}
		m_context.SetCullingMode(platform::Context::FaceCullingMode::Back);


		// Fill GBuffer:
		RenderToGBuffer(mainCam);

		m_outputTargetSet->AttachColorTargets(0, 0, true);

		// Clear the currently bound targets
		m_context.ClearTargets(platform::Context::ClearTarget::ColorDepth);

		// Render additive contributions:
		m_context.SetBlendMode(platform::Context::BlendMode::One,platform::Context::BlendMode::One);

		if (!deferredLights.empty())
		{
			// Render the first light
			RenderDeferredLight(deferredLights[0]);

			// Set the blend mode: Light contributions are additive
			m_context.SetBlendMode(platform::Context::BlendMode::One, platform::Context::BlendMode::One);

			m_context.SetDepthMode(platform::Context::DepthMode::GEqual);

			for (int i = 1; i < deferredLights.size(); i++)
			{
				// Select face culling:
				if (deferredLights[i]->Type() == LIGHT_AMBIENT_COLOR ||
					deferredLights[i]->Type() == LIGHT_AMBIENT_IBL ||
					deferredLights[i]->Type() == LIGHT_DIRECTIONAL)
				{
					m_context.SetCullingMode(platform::Context::FaceCullingMode::Back);
				}
				else
				{
					// For 3D deferred light meshes, we render back faces so something is visible even while we're 
					// inside the mesh		
					m_context.SetCullingMode(platform::Context::FaceCullingMode::Front);
				}

				RenderDeferredLight(deferredLights[i]);
			}
		}
		m_context.SetCullingMode(platform::Context::FaceCullingMode::Back);

		// Render the skybox on top of the frame:
		m_context.SetBlendMode(platform::Context::BlendMode::Disabled, platform::Context::BlendMode::Disabled);

		RenderSkybox(CoreEngine::GetSceneManager()->GetSkybox());

		// Additively blit the emissive GBuffer texture to screen:
		m_context.SetBlendMode(platform::Context::BlendMode::One, platform::Context::BlendMode::One);

		Blit(
			mainCam->GetTextureTargetSet().ColorTarget(Material::MatEmissive).GetTexture(),
			*m_outputTargetSet.get(),
			m_blitShader);

		m_context.SetBlendMode(platform::Context::BlendMode::Disabled, platform::Context::BlendMode::Disabled);

		// Post process finished frame:
		std::shared_ptr<Shader> finalFrameShader = nullptr; // Reference updated in ApplyPostFX...
		m_postFXManager->ApplyPostFX(finalFrameShader);

		// Cleanup:
		m_context.SetDepthMode(platform::Context::DepthMode::Less);
		m_context.SetCullingMode(platform::Context::FaceCullingMode::Back);

		// Blit results to screen (Using the final post processing shader pass supplied by the PostProcessingManager):
		BlitToScreen(m_outputTargetSet->ColorTarget(0).GetTexture(), finalFrameShader);
		
		// Display the final frame:
		m_context.SwapWindow();
	}


	void RenderManager::RenderLightShadowMap(std::shared_ptr<Light> light)
	{
		std::shared_ptr<Camera> shadowCam = light->GetShadowMap()->ShadowCamera();

		// Light shader setup:
		std::shared_ptr<Shader> lightShader = shadowCam->GetRenderShader();
		lightShader->Bind(true);
		
		if (light->Type() == LIGHT_POINT)
		{
			mat4 shadowCamProjection = shadowCam->Projection();
			vec3 lightWorldPos = light->GetTransform().WorldPosition();

			mat4 const* cubeMap_vps = shadowCam->CubeViewProjection();

			lightShader->SetUniform("shadowCamCubeMap_vp", &cubeMap_vps[0][0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f, 6);
			lightShader->SetUniform("lightWorldPos", &lightWorldPos.x, platform::Shader::UNIFORM_TYPE::Vec3f);
			lightShader->SetUniform("shadowCam_near", &shadowCam->Near(), platform::Shader::UNIFORM_TYPE::Float);
			lightShader->SetUniform("shadowCam_far", &shadowCam->Far(), platform::Shader::UNIFORM_TYPE::Float);
		}

		light->GetShadowMap()->GetTextureTargetSet().AttachDepthStencilTarget(true);

		// Clear the currently bound FBO	
		m_context.ClearTargets(platform::Context::ClearTarget::Depth);

		// Loop through each mesh:			
		vector<std::shared_ptr<gr::Mesh>> const* meshes = CoreEngine::GetSceneManager()->GetRenderMeshes(nullptr); // Get ALL meshes
		unsigned int numMeshes	= (unsigned int)meshes->size();
		for (unsigned int j = 0; j < numMeshes; j++)
		{
			std::shared_ptr<gr::Mesh> currentMesh = meshes->at(j);

			currentMesh->Bind(true);

			switch (light->Type())
			{
			case LIGHT_DIRECTIONAL:
			{
				mat4 mvp			= shadowCam->ViewProjection() * currentMesh->GetTransform().Model();
				lightShader->SetUniform("in_mvp",	&mvp[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
			}
			break;

			case LIGHT_POINT:
			{
				mat4 model = currentMesh->GetTransform().Model();
				lightShader->SetUniform("in_model",	&model[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
			}
			break;

			case LIGHT_AMBIENT_COLOR:
			case LIGHT_AMBIENT_IBL:
			case LIGHT_AREA:
			case LIGHT_SPOT:
			case LIGHT_TUBE:
			default:
				return; // This should never happen...
			}
			// TODO: ^^^^ Only upload these matrices if they've changed			

			// Draw!
			glDrawElements(GL_TRIANGLES, 
				(GLsizei)currentMesh->NumIndices(),
				GL_UNSIGNED_INT, 
				(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
		}
	}


	void SaberEngine::RenderManager::RenderToGBuffer(std::shared_ptr<Camera> const renderCam)
	{
		gr::TextureTargetSet const& renderTargetSet = renderCam->GetTextureTargetSet();
		
		renderTargetSet.AttachColorDepthStencilTargets(0, 0, true);		
		
		// Clear the currently bound target
		m_context.ClearTargets(platform::Context::ClearTarget::ColorDepth);

		// Assemble common (model independent) matrices:
		mat4 m_view			= renderCam->View();

		// Loop by material (+shader), mesh:
		std::unordered_map<string, std::shared_ptr<Material>> const sceneMaterials = 
			CoreEngine::GetSceneManager()->GetMaterials();

		for (std::pair<string, std::shared_ptr<Material>> currentElement : sceneMaterials)
		{
			// Setup the current material and shader:
			std::shared_ptr<Material> currentMaterial = currentElement.second;
			std::shared_ptr<Shader> currentShader = renderCam->GetRenderShader();

			vector<std::shared_ptr<gr::Mesh>> const* meshes;

			// Bind:
			currentShader->Bind(true);
			currentMaterial->BindAllTextures(Material::MatAlbedo, true);

			// Upload material properties:
			currentShader->SetUniform(
				Material::k_MatPropNames[Material::MatProperty0].c_str(), 
				&currentMaterial->Property(Material::MatProperty0).x, 
				platform::Shader::UNIFORM_TYPE::Vec4f);
			currentShader->SetUniform("in_view", &m_view[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);

			// Get all meshes that use the current material
			meshes = CoreEngine::GetSceneManager()->GetRenderMeshes(currentMaterial);

			// Loop through each mesh:			
			unsigned int numMeshes	= (unsigned int)meshes->size();
			for (unsigned int j = 0; j < numMeshes; j++)
			{
				std::shared_ptr<gr::Mesh> const currentMesh = meshes->at(j);

				currentMesh->Bind(true);

				// Assemble model-specific matrices:
				mat4 model			= currentMesh->GetTransform().Model();
				mat4 modelRotation	= currentMesh->GetTransform().Model(WORLD_ROTATION);
				mat4 mvp			= renderCam->ViewProjection() * model;

				// Upload mesh-specific matrices:
				currentShader->SetUniform("in_model", &model[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
				currentShader->SetUniform("in_modelRotation", &modelRotation[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
				currentShader->SetUniform("in_mvp", &mvp[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
				// TODO: Only upload these matrices if they've changed ^^^^

				// Draw!
				glDrawElements(GL_TRIANGLES, 
					(GLsizei)currentMesh->NumIndices(), 
					GL_UNSIGNED_INT, 
					(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
			}
		} // End Material loop
	}


	void SaberEngine::RenderManager::RenderDeferredLight(std::shared_ptr<Light> deferredLight)
	{
		std::shared_ptr<Camera> gBufferCam = CoreEngine::GetSceneManager()->GetMainCamera();

		// Bind:
		std::shared_ptr<Shader> currentShader = deferredLight->GetDeferredLightShader();

		currentShader->Bind(true);

		// Bind GBuffer textures
		gr::TextureTargetSet& gbufferTextures = gBufferCam->GetTextureTargetSet();
		for (size_t gBufferTexSlot = 0; gBufferTexSlot < gbufferTextures.ColorTargets().size(); gBufferTexSlot++)
		{
			std::shared_ptr<gr::Texture>& tex = gbufferTextures.ColorTarget(gBufferTexSlot).GetTexture();
			if (tex != nullptr)
			{
				tex->Bind((uint32_t)gBufferTexSlot, true);
			}
		}
		
		// Assemble common (model independent) matrices:
		const bool hasShadowMap = deferredLight->GetShadowMap() != nullptr;
			
		mat4 model			= deferredLight->GetTransform().Model();
		mat4 m_view			= gBufferCam->View();
		mat4 mv				= m_view * model;
		mat4 mvp			= gBufferCam->ViewProjection() * deferredLight->GetTransform().Model();
		vec3 cameraPosition = gBufferCam->GetTransform()->WorldPosition();

		currentShader->SetUniform("in_model", &model[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
		currentShader->SetUniform("in_view", &m_view[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
		currentShader->SetUniform("in_mv", &mv[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
		currentShader->SetUniform("in_mvp", &mvp[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
		currentShader->SetUniform("cameraWPos", &cameraPosition, platform::Shader::UNIFORM_TYPE::Vec3f);
		// TODO: Only upload these matrices if they've changed ^^^^
		// TODO: Break this out into a function: ALL of our render functions have a similar setup		

		// Light properties:
		currentShader->SetUniform("lightColor", &deferredLight->Color().r, platform::Shader::UNIFORM_TYPE::Vec3f);

		switch (deferredLight->Type())
		{
		case LIGHT_AMBIENT_IBL:
		{
			// Bind IBL cubemaps:
			std::shared_ptr<gr::Texture> IEMCubemap =
				dynamic_cast<ImageBasedLight*>(deferredLight.get())->GetIEMTexture();

			if (IEMCubemap != nullptr)
			{
				IEMCubemap->Bind(Material::CubeMap0, true);
			}

			std::shared_ptr<gr::Texture> PMREM_Cubemap =
				dynamic_cast<ImageBasedLight*>(deferredLight.get())->GetPMREMTexture();

			if (PMREM_Cubemap != nullptr)
			{
				PMREM_Cubemap->Bind(Material::CubeMap1, true);
			}

			// Bind BRDF Integration map:
			std::shared_ptr<gr::Texture> BRDFIntegrationMap =
				dynamic_cast<ImageBasedLight*>(deferredLight.get())->GetBRDFIntegrationMap();
			if (BRDFIntegrationMap != nullptr)
			{
				BRDFIntegrationMap->Bind(Material::Tex7, true);
			}
		}
			break;

		case LIGHT_DIRECTIONAL:
		{
			currentShader->SetUniform(
				"keylightWorldDir", 
				&deferredLight->GetTransform().Forward().x, 
				platform::Shader::UNIFORM_TYPE::Vec3f);

			vec3 keylightViewDir = glm::normalize(m_view * vec4(deferredLight->GetTransform().Forward(), 0.0f));
			currentShader->SetUniform("keylightViewDir", &keylightViewDir.x, platform::Shader::UNIFORM_TYPE::Vec3f);
		}
			break;

		case LIGHT_AMBIENT_COLOR:
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_AREA:
		case LIGHT_TUBE:
		{
			currentShader->SetUniform(
				"lightWorldPos", 
				&deferredLight->GetTransform().WorldPosition().x, 
				platform::Shader::UNIFORM_TYPE::Vec3f);

			// TODO: Can we just upload this once when the light is created (and its shader is created)? 
			// (And also update it if the light is ever moved)
		}
			break;
		default:
			break;
		}

		// Shadow properties:
		std::shared_ptr<ShadowMap> activeShadowMap = deferredLight->GetShadowMap();

		mat4 shadowCam_vp(1.0);
		if (activeShadowMap != nullptr)
		{
			std::shared_ptr<Camera> shadowCam = activeShadowMap->ShadowCamera();

			if (shadowCam != nullptr)
			{
				// Upload common shadow properties:
				currentShader->SetUniform("shadowCam_vp", &shadowCam->ViewProjection()[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);
				
				currentShader->SetUniform("maxShadowBias", &activeShadowMap->MaxShadowBias(), platform::Shader::UNIFORM_TYPE::Float);
				currentShader->SetUniform("minShadowBias", &activeShadowMap->MinShadowBias(), platform::Shader::UNIFORM_TYPE::Float);

				currentShader->SetUniform("shadowCam_near", &shadowCam->Near(),	platform::Shader::UNIFORM_TYPE::Float);
				currentShader->SetUniform("shadowCam_far", &shadowCam->Far(), platform::Shader::UNIFORM_TYPE::Float);

				// Bind shadow depth textures:
				std::shared_ptr<gr::Texture> depthTexture = 
					activeShadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture();

				switch (deferredLight->Type())
				{
				case LIGHT_DIRECTIONAL:
				{
					if (depthTexture)
					{
						depthTexture->Bind(Material::Depth0, true);
					}
				}
				break;

				case LIGHT_POINT:
				{
					if (depthTexture)
					{
						depthTexture->Bind(Material::CubeMap0, true);
					}
				}
				break;

				// Other light types don't support shadows, yet:
				case LIGHT_AMBIENT_COLOR:
				case LIGHT_AMBIENT_IBL:
				case LIGHT_AREA:
				case LIGHT_SPOT:
				case LIGHT_TUBE:
				default:
					break;
				}

				vec4 texelSize(0, 0, 0, 0);
				if (depthTexture != nullptr)
				{
					texelSize = depthTexture->GetTexelDimenions();
				}
				currentShader->SetUniform("texelSize", &texelSize.x, platform::Shader::UNIFORM_TYPE::Vec4f);
			}
		}
		
		deferredLight->DeferredMesh()->Bind(true);

		// Draw!
		glDrawElements(GL_TRIANGLES,
			(GLsizei)deferredLight->DeferredMesh()->NumIndices(),
			GL_UNSIGNED_INT, 
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
	}


	void SaberEngine::RenderManager::RenderSkybox(std::shared_ptr<Skybox> skybox)
	{
		if (skybox == nullptr)
		{
			return;
		}

		std::shared_ptr<Camera> renderCam = CoreEngine::GetSceneManager()->GetMainCamera();

		std::shared_ptr<Shader> currentShader = skybox->GetSkyShader();

		std::shared_ptr<gr::Texture> skyboxCubeMap = skybox->GetSkyTexture();

		// GBuffer depth
		std::shared_ptr<gr::Texture> depthTexture = renderCam->GetTextureTargetSet().DepthStencilTarget().GetTexture();		

		// Bind shader and texture:
		currentShader->Bind(true);
		skyboxCubeMap->Bind(Material::CubeMap0, true);

		if (depthTexture)
		{
			depthTexture->Bind(Material::GBufferDepth, true);
		}

		skybox->GetSkyMesh()->Bind(true);

		// Assemble common (model independent) matrices:
		mat4 inverseViewProjection = 
			glm::inverse(renderCam->ViewProjection()); // TODO: Only compute this if something has changed

		currentShader->SetUniform("in_inverse_vp", &inverseViewProjection[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);

		// Draw!
		glDrawElements(
			GL_TRIANGLES,									// GLenum mode
			(GLsizei)skybox->GetSkyMesh()->NumIndices(),	// GLsizei count
			GL_UNSIGNED_INT,								// GLenum type
			(void*)(0));									// const GLvoid* indices
	}


	void SaberEngine::RenderManager::BlitToScreen()
	{
		m_defaultTargetSet->AttachColorDepthStencilTargets(0, 0, true);
		m_context.ClearTargets(platform::Context::ClearTarget::ColorDepth);

		m_blitShader->Bind(true);
		m_screenAlignedQuad->Bind(true);

		glDrawElements(
			GL_TRIANGLES, 
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT, 
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
	}


	void SaberEngine::RenderManager::BlitToScreen(std::shared_ptr<gr::Texture>& texture, std::shared_ptr<Shader> blitShader)
	{
		m_defaultTargetSet->AttachColorDepthStencilTargets(0, 0, true);
		m_context.ClearTargets(platform::Context::ClearTarget::ColorDepth);

		blitShader->Bind(true);

		texture->Bind(Material::GBufferAlbedo, true); // TODO: Define a better texture slot name for this

		m_screenAlignedQuad->Bind(true);

		glDrawElements(
			GL_TRIANGLES,
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT,
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
	}


	void SaberEngine::RenderManager::Blit(
		std::shared_ptr<gr::Texture> const& srcTex,
		gr::TextureTargetSet const& dstTargetSet,
		std::shared_ptr<Shader> shader)
	{
		dstTargetSet.AttachColorTargets(0, 0, true);

		// Bind the blit shader and screen aligned quad:
		shader->Bind(true);
		m_screenAlignedQuad->Bind(true);

		// Bind the source texture into the slot specified in the blit shader:
		// Note: Blit shader reads from this texture unit (for now)
		srcTex->Bind(Material::GBufferAlbedo, true);
		
		glDrawElements(
			GL_TRIANGLES,
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT, 
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
	}


	void SaberEngine::RenderManager::Initialize()
	{
		SceneManager* sceneManager	= CoreEngine::GetSceneManager();
		unsigned int numMaterials	= sceneManager->NumMaterials();

		// Legacy forward rendering params:
		std::shared_ptr<Light const> ambientLight	= nullptr;
		vec3 const* ambientColor	= nullptr;
		if ((ambientLight = CoreEngine::GetSceneManager()->GetAmbientLight()) != nullptr)
		{
			ambientColor = &CoreEngine::GetSceneManager()->GetAmbientLight()->Color();
		}

		vec3 const* keyDir			= nullptr;
		vec3 const* keyCol			= nullptr;
		std::shared_ptr<Light const> keyLight = nullptr;
		if ((keyLight = CoreEngine::GetSceneManager()->GetKeyLight()) != nullptr)
		{
			keyDir = &CoreEngine::GetSceneManager()->GetKeyLight()->GetTransform().Forward();
			keyCol = &CoreEngine::GetSceneManager()->GetKeyLight()->Color();
		}

		LOG("Uploading light and matrix data to shaders");
		#if defined(DEBUG_RENDERMANAGER_SHADER_LOGGING)
			LOG("Ambient: " + to_string(ambientColor->r) + ", " + to_string(ambientColor->g) + ", " + to_string(ambientColor->b));
			LOG("Key Dir: " + to_string(keyDir->x) + ", " + to_string(keyDir->y) + ", " + to_string(keyDir->z));
			LOG("Key Col: " + to_string(keyCol->r) + ", " + to_string(keyCol->g) + ", " + to_string(keyCol->b));
		#endif

		vec4 screenParams = vec4(m_xRes, m_yRes, 1.0f / m_xRes, 1.0f / m_yRes);
		vec4 projectionParams = vec4(
			1.0f, 
			CoreEngine::GetSceneManager()->GetMainCamera()->Near(), 
			CoreEngine::GetSceneManager()->GetMainCamera()->Far(), 
			1.0f / CoreEngine::GetSceneManager()->GetMainCamera()->Far());

		// Add all Material Shaders to a list:
		vector<std::shared_ptr<Shader>> shaders;
		std::unordered_map<string, std::shared_ptr<Material>> const sceneMaterials = 
			CoreEngine::GetSceneManager()->GetMaterials();

		for (std::pair<string, std::shared_ptr<Material>> currentElement : sceneMaterials)
		{
			std::shared_ptr<Material> currentMaterial = currentElement.second;
			if (currentMaterial->GetShader() != nullptr)
			{
				shaders.push_back(currentMaterial->GetShader());
			}			
		}

		// Add all Camera Shaders:	
		for (int i = 0; i < CAMERA_TYPE_COUNT; i++)
		{
			vector<std::shared_ptr<Camera>> cameras = CoreEngine::GetSceneManager()->GetCameras((CAMERA_TYPE)i);
			for (int currentCam = 0; currentCam < cameras.size(); currentCam++)
			{
				if (cameras.at(currentCam)->GetRenderShader())
				{
					shaders.push_back(cameras.at(currentCam)->GetRenderShader());
				}
			}
		}
			
		// Add deferred light Shaders
		vector<std::shared_ptr<Light>> const* deferredLights = &CoreEngine::GetSceneManager()->GetDeferredLights();
		for (size_t currentLight = 0; currentLight < deferredLights->size(); currentLight++)
		{
			shaders.push_back(deferredLights->at(currentLight)->GetDeferredLightShader());
		}

		// Add skybox shader:
		std::shared_ptr<Skybox> skybox = CoreEngine::GetSceneManager()->GetSkybox();
		if (skybox && skybox->GetSkyShader())
		{
			shaders.push_back(skybox->GetSkyShader());
		}		
		
		// Add RenderManager shaders:
		shaders.push_back(m_blitShader);

		// Configure all of the shaders:
		for (unsigned int i = 0; i < (int)shaders.size(); i++)
		{
			shaders.at(i)->Bind(true);

			// Upload light direction (world space) and color, and ambient light color:
			if (ambientLight != nullptr)
			{
				shaders.at(i)->SetUniform("ambientColor", &(ambientColor->r), platform::Shader::UNIFORM_TYPE::Vec3f);
			}

			// TODO: Shift more value uploads into the shader creation flow
			
			// Other params:
			shaders.at(i)->SetUniform("screenParams", &(screenParams.x), platform::Shader::UNIFORM_TYPE::Vec4f);
			shaders.at(i)->SetUniform("projectionParams", &(projectionParams.x), platform::Shader::UNIFORM_TYPE::Vec4f);

			float emissiveIntensity = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultSceneEmissiveIntensity");
			shaders.at(i)->SetUniform("emissiveIntensity", &emissiveIntensity, platform::Shader::UNIFORM_TYPE::Float);
			// TODO: Load this from .FBX file, and set the cached value here


			// Upload matrices:
			mat4 m_projection = sceneManager->GetMainCamera()->Projection();
			shaders.at(i)->SetUniform("in_projection", &m_projection[0][0], platform::Shader::UNIFORM_TYPE::Matrix4x4f);

			shaders.at(i)->Bind(false);
		}

		// Initialize PostFX:
		m_postFXManager->Initialize(m_outputTargetSet->ColorTarget(0));
	}


}


