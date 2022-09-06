#include <string>
#include <unordered_map>

// TODO: Remove these!!!!!!!!!!!
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "RenderManager.h"
#include "CoreEngine.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Mesh.h"
#include "Transform.h"
#include "Material.h"
#include "Texture.h"
#include "DebugConfiguration.h"
#include "Skybox.h"
#include "Camera.h"
#include "ImageBasedLight.h"
#include "PostFXManager.h"
#include "ShadowMap.h"
#include "Scene.h"
#include "EventManager.h"
#include "Sampler.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"

using gr::Material;
using gr::Texture;
using gr::Shader;
using gr::Sampler;
using gr::Light;
using gr::ShadowMap;
using gr::Transform;
using gr::GBufferGraphicsSystem;
using gr::DeferredLightingGraphicsSystem;
using gr::ShadowsGraphicsSystem;
using gr::RenderStage;
using gr::TextureTargetSet;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::string;
using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;


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
		m_xRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		m_yRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");

		// Default target set:
		m_defaultTargetSet = std::make_shared<gr::TextureTargetSet>("Default target");
		m_defaultTargetSet->Viewport() = { 0, 0, (uint32_t)m_xRes, (uint32_t)m_yRes };
		m_defaultTargetSet->CreateColorTargets(); // Default framebuffer has no texture targets

		// Output target:		
		Texture::TextureParams mainTargetParams;
		mainTargetParams.m_width = m_xRes;
		mainTargetParams.m_height = m_yRes;
		mainTargetParams.m_faces = 1;
		mainTargetParams.m_texUse = Texture::TextureUse::ColorTarget;
		mainTargetParams.m_texDimension = Texture::TextureDimension::Texture2D;
		mainTargetParams.m_texFormat = Texture::TextureFormat::RGBA32F;
		mainTargetParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		mainTargetParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		mainTargetParams.m_texturePath = "RenderManagerFrameOutput";

		std::shared_ptr<gr::Texture> outputTexture = std::make_shared<gr::Texture>(mainTargetParams);

		m_mainTargetSet = std::make_shared<gr::TextureTargetSet>("Main target");
		m_mainTargetSet->ColorTarget(0) = outputTexture;

		m_mainTargetSet->CreateColorTargets();
		
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
		m_mainTargetSet = nullptr;
		m_screenAlignedQuad = nullptr;
		m_postFXManager = nullptr;
	}


	void RenderManager::Update()
	{
		Render();

		// TEMP HAX: Ensure the culling mode is reset after Render()...
		m_context.SetCullingMode(platform::Context::FaceCullingMode::Back);


		gr::TextureTargetSet& deferredLightTextureTargetSet = 
			GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();
		deferredLightTextureTargetSet.AttachColorTargets(0, 0, true);


		// Render the skybox on top of the frame:
		m_context.SetBlendMode(platform::Context::BlendMode::Disabled, platform::Context::BlendMode::Disabled);

		RenderSkybox(CoreEngine::GetSceneManager()->GetSkybox());

		// Additively blit the emissive GBuffer texture to screen:
		m_context.SetBlendMode(platform::Context::BlendMode::One, platform::Context::BlendMode::One);


		Blit(
			GetGraphicsSystem<gr::GBufferGraphicsSystem>()->GetFinalTextureTargetSet().ColorTarget(Material::MatEmissive).GetTexture(),
			deferredLightTextureTargetSet,
			m_blitShader);




		m_context.SetBlendMode(platform::Context::BlendMode::Disabled, platform::Context::BlendMode::Disabled);

		// Post process finished frame:
		std::shared_ptr<Shader> finalFrameShader = nullptr; // Reference updated in ApplyPostFX...
		m_postFXManager->ApplyPostFX(finalFrameShader);

		// Cleanup:
		m_context.SetDepthMode(platform::Context::DepthMode::Less);
		m_context.SetCullingMode(platform::Context::FaceCullingMode::Back);


		// Blit results to screen (Using the final post processing shader pass supplied by the PostProcessingManager):
		BlitToScreen(deferredLightTextureTargetSet.ColorTarget(0).GetTexture(), finalFrameShader);



		// Display the final frame:
		m_context.SwapWindow();
	}


	void RenderManager::Render()
	{
		// TODO: This should be an API-agnostic function bound at runtime, to handle different submission techniques
		// between APIs
		// -> Just handle OpenGL for now...

		// Update the graphics systems:
		for (std::shared_ptr<gr::GraphicsSystem> curGS : m_graphicsSystems)
		{
			curGS->PreRender();
		}

		// Render each stage:
		const size_t numGS = m_pipeline.GetNumberGraphicsSystems();
		for (size_t gsIdx = 0; gsIdx < numGS; gsIdx++)
		{
			// RenderDoc markers: Graphics system group name
			glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, m_pipeline.GetPipeline()[gsIdx].GetName().c_str());

			const size_t numStages = m_pipeline.GetNumberOfGraphicsSystemStages(gsIdx);
			for (size_t stage = 0; stage < numStages; stage++)
			{
				RenderStage const* renderStage = m_pipeline.GetPipeline()[gsIdx][stage];

				// RenderDoc makers: Render stage name
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, renderStage->GetName().c_str());

				RenderStage::RenderStageParams const& renderStageParams = renderStage->GetStageParams();

				// Attach the stage targets:
				TextureTargetSet const& stageTargets = renderStage->GetTextureTargetSet();
				stageTargets.AttachColorDepthStencilTargets(0, 0, true);
				// TODO: Handle selection of face, miplevel -> Via stage params?

				// Configure the shader:
				std::shared_ptr<Shader const> stageShader = renderStage->GetStageShader();
				stageShader->Bind(true);
				// TODO: Use shaders from materials in some cases?

				// Set per-frame stage shader uniforms:
				vector<RenderStage::StageShaderUniform> const& stagePerFrameShaderUniforms =
					renderStage->GetPerFrameShaderUniforms();
				for (RenderStage::StageShaderUniform curUniform : stagePerFrameShaderUniforms)
				{
					stageShader->SetUniform(
						curUniform.m_uniformName, curUniform.m_value, curUniform.m_type, curUniform.m_count);
				}

				// Set camera params:
				std::shared_ptr<Camera const> stageCam = renderStage->GetStageCamera();
				stageShader->SetUniform(
					"in_view", 
					&stageCam->GetViewMatrix()[0][0],
					platform::Shader::UniformType::Matrix4x4f,
					1);
				stageShader->SetUniform(
					"cameraWPos", 
					&stageCam->GetTransform()->GetWorldPosition(), 
					platform::Shader::UniformType::Vec3f, 
					1);
				// TODO: These should be set via a general camera param block, shared between stages that need it
				
				// Configure the context:
				m_context.ClearTargets(renderStageParams.m_targetClearMode);
				m_context.SetCullingMode(renderStageParams.m_faceCullingMode);
				m_context.SetBlendMode(renderStageParams.m_srcBlendMode, renderStageParams.m_dstBlendMode);
				m_context.SetDepthMode(renderStageParams.m_depthMode);
				
				// Render stage geometry:
				std::vector<std::shared_ptr<gr::Mesh>> const* meshes = renderStage->GetGeometryBatches();
				SEAssert("Stage does not have any geometry to render", meshes != nullptr);
				size_t meshIdx = 0;
				for (std::shared_ptr<gr::Mesh> mesh : *meshes)
				{
					mesh->Bind(true);

					shared_ptr<gr::Material> meshMaterial = mesh->MeshMaterial();
					if (meshMaterial != nullptr && 
						renderStageParams.m_stageType != RenderStage::RenderStageType::DepthOnly)
					{
						// TODO: Is there a more elegant way to handle this?
						meshMaterial->BindToShader(stageShader);
					}


					// TODO: Support instancing. For now, just upload per-mesh parameters as a workaround...
					std::vector<std::vector<RenderStage::StageShaderUniform>> const& perMeshUniforms = 
						renderStage->GetPerMeshPerFrameShaderUniforms();
					if (perMeshUniforms.size() > 0)
					{
						for (size_t curUniform = 0; curUniform < perMeshUniforms[meshIdx].size(); curUniform++)
						{
							stageShader->SetUniform(
								perMeshUniforms[meshIdx][curUniform].m_uniformName,
								perMeshUniforms[meshIdx][curUniform].m_value,
								perMeshUniforms[meshIdx][curUniform].m_type,
								perMeshUniforms[meshIdx][curUniform].m_count);
						}
					}


					// Assemble and upload mesh-specific matrices:
					const mat4 model = mesh->GetTransform().Model();
					const mat4 modelRotation = mesh->GetTransform().Model(Transform::WorldRotation);
					const mat4 mv = stageCam->GetViewMatrix() * model;
					const mat4 mvp = stageCam->GetViewProjectionMatrix() * model;
					
					stageShader->SetUniform("in_model", &model[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					stageShader->SetUniform(
						"in_modelRotation", &modelRotation[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					// ^^ TODO: in_modelRotation isn't always used...
					stageShader->SetUniform("in_mv", &mv[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					stageShader->SetUniform("in_mvp", &mvp[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					// TODO: Figure out a more generic solution for this?

					// Draw!
					glDrawElements(
						GL_TRIANGLES,
						(GLsizei)mesh->NumIndices(),
						GL_UNSIGNED_INT,
						(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

					meshIdx++;
				} // meshes

				glPopDebugGroup();
			}

			glPopDebugGroup();
		}
	}


	void SaberEngine::RenderManager::RenderSkybox(std::shared_ptr<Skybox> skybox)
	{
		// RenderDoc markers: Graphics system group name
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Skybox stage");
		
		

		if (skybox == nullptr)
		{
			return;
		}

		std::shared_ptr<Camera> renderCam = CoreEngine::GetSceneManager()->GetMainCamera();

		std::shared_ptr<Shader> currentShader = skybox->GetSkyShader();

		std::shared_ptr<gr::Texture> skyboxCubeMap = skybox->GetSkyTexture();


		// GBuffer depth
		std::shared_ptr<gr::Texture> depthTexture = 
			m_pipeline.GetPipeline()[0][0]->GetTextureTargetSet().DepthStencilTarget().GetTexture(); // HAX!!!!!!!!!!!!!!!!!!!!!!



		// Bind shader and texture:
		currentShader->Bind(true);
		skyboxCubeMap->Bind(Material::CubeMap0, true);

		Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear)->Bind(Material::CubeMap0, true);

		SEAssert("Depth texture is null", depthTexture != nullptr);

		depthTexture->Bind(Material::GBufferDepth, true);
		Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear)->Bind(Material::GBufferDepth, true);

		skybox->GetSkyMesh()->Bind(true);

		// Assemble common (model independent) matrices:
		mat4 inverseViewProjection = 
			glm::inverse(renderCam->GetViewProjectionMatrix()); // TODO: Only compute this if something has changed

		currentShader->SetUniform("in_inverse_vp", &inverseViewProjection[0][0], platform::Shader::UniformType::Matrix4x4f, 1);

		// Draw!
		glDrawElements(
			GL_TRIANGLES,									// GLenum mode
			(GLsizei)skybox->GetSkyMesh()->NumIndices(),	// GLsizei count
			GL_UNSIGNED_INT,								// GLenum type
			(void*)(0));									// const GLvoid* indices

		glPopDebugGroup();
	}


	void SaberEngine::RenderManager::BlitToScreen()
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Blit to screen stage");

		m_defaultTargetSet->AttachColorDepthStencilTargets(0, 0, true);
		m_context.ClearTargets(platform::Context::ClearTarget::ColorDepth);

		m_blitShader->Bind(true);
		m_screenAlignedQuad->Bind(true);

		glDrawElements(
			GL_TRIANGLES, 
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT, 
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

		glPopDebugGroup();
	}


	void SaberEngine::RenderManager::BlitToScreen(std::shared_ptr<gr::Texture>& texture, std::shared_ptr<Shader> blitShader)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Blit to screen with texture and shader stage");

		m_defaultTargetSet->AttachColorDepthStencilTargets(0, 0, true);
		m_context.ClearTargets(platform::Context::ClearTarget::ColorDepth);

		blitShader->Bind(true);

		texture->Bind(Material::GBufferAlbedo, true); // TODO: Define a better texture slot name for this

		Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear)->Bind(Material::GBufferAlbedo, true);

		m_screenAlignedQuad->Bind(true);

		glDrawElements(
			GL_TRIANGLES,
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT,
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

		glPopDebugGroup();
	}


	void SaberEngine::RenderManager::Blit(
		std::shared_ptr<gr::Texture> const& srcTex,
		gr::TextureTargetSet const& dstTargetSet,
		std::shared_ptr<Shader> shader)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Blit to screen from texture to target set with shader stage");

		dstTargetSet.AttachColorTargets(0, 0, true);

		// Bind the blit shader and screen aligned quad:
		shader->Bind(true);
		m_screenAlignedQuad->Bind(true);

		// Bind the source texture into the slot specified in the blit shader:
		// Note: Blit shader reads from this texture unit (for now)
		srcTex->Bind(Material::GBufferAlbedo, true);

		Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear)->Bind(Material::GBufferAlbedo, true);
		
		glDrawElements(
			GL_TRIANGLES,
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT, 
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

		glPopDebugGroup();
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
			ambientColor = &CoreEngine::GetSceneManager()->GetAmbientLight()->GetColor();
		}

		vec3 const* keyDir			= nullptr;
		vec3 const* keyCol			= nullptr;
		std::shared_ptr<Light const> keyLight = nullptr;
		if ((keyLight = CoreEngine::GetSceneManager()->GetKeyLight()) != nullptr)
		{
			keyDir = &CoreEngine::GetSceneManager()->GetKeyLight()->GetTransform().Forward();
			keyCol = &CoreEngine::GetSceneManager()->GetKeyLight()->GetColor();
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

		// TODO: Individual stages/materials/etc should be configuring shader values, not the render manager!
		
		// Configure all of the shaders:
		for (unsigned int i = 0; i < (int)shaders.size(); i++)
		{
			shaders.at(i)->Bind(true);

			// Upload light direction (world space) and color, and ambient light color:
			if (ambientLight != nullptr)
			{
				shaders.at(i)->SetUniform("ambientColor", &(ambientColor->r), platform::Shader::UniformType::Vec3f, 1);
			}

			// TODO: Shift more value uploads into the shader creation flow
			
			// Other params:
			shaders.at(i)->SetUniform("screenParams", &(screenParams.x), platform::Shader::UniformType::Vec4f, 1);
			shaders.at(i)->SetUniform("projectionParams", &(projectionParams.x), platform::Shader::UniformType::Vec4f, 1);


			// Upload matrices:
			mat4 m_projection = sceneManager->GetMainCamera()->GetProjectionMatrix();
			shaders.at(i)->SetUniform("in_projection", &m_projection[0][0], platform::Shader::UniformType::Matrix4x4f, 1);

			shaders.at(i)->Bind(false);
		}

		// Add graphics systems, in order:
		m_graphicsSystems.emplace_back(make_shared<GBufferGraphicsSystem>("GBufferGraphicsSystem"));
		m_graphicsSystems.emplace_back(make_shared<ShadowsGraphicsSystem>("ShadowsGraphicsSystem"));
		m_graphicsSystems.emplace_back(make_shared<DeferredLightingGraphicsSystem>("DeferredLightingGraphicsSystem"));		
		// NOTE: Adding a new graphics system? Don't forget to add a new template instantiation below GetGraphicsSystem()
		
		// Create each graphics system in turn:
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			m_graphicsSystems[i]->Create(m_pipeline.AddNewStagePipeline(m_graphicsSystems[i]->GetName()));
		}



		// TEMP HAX: Initialize PostFX with the most up-to-date texture target set from the new system
		m_postFXManager->Initialize(
			GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet().ColorTarget(0));
	}


	template <typename T>
	std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem()
	{
		// TODO: A linear search isn't optimal here, but there aren't many graphics systems in practice so ok for now
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			if (dynamic_cast<T*>(m_graphicsSystems[i].get()) != nullptr)
			{
				return m_graphicsSystems[i];
			}
		}

		SEAssert("Graphics system not found", false);
		return nullptr;
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem<GBufferGraphicsSystem>();
	template std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem<DeferredLightingGraphicsSystem>();
	template std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem<ShadowsGraphicsSystem>();
}


