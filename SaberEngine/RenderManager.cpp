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
#include "Camera.h"
#include "ImageBasedLight.h"
#include "ShadowMap.h"
#include "Scene.h"
#include "EventManager.h"
#include "Sampler.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"

using gr::Material;
using gr::Texture;
using gr::Shader;
using gr::Sampler;
using gr::Light;
using gr::ShadowMap;
using gr::Transform;
using gr::GBufferGraphicsSystem;
using gr::DeferredLightingGraphicsSystem;
using gr::GraphicsSystem;
using gr::ShadowsGraphicsSystem;
using gr::SkyboxGraphicsSystem;
using gr::BloomGraphicsSystem;
using gr::TonemappingGraphicsSystem;
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

		

		// Default target set:
		m_defaultTargetSet = std::make_shared<gr::TextureTargetSet>("Default target");
		m_defaultTargetSet->Viewport() = 
		{ 
			0, 
			0, 
			(uint32_t)CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes"),
			(uint32_t)CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes")
		};
		m_defaultTargetSet->CreateColorTargets(); // Default framebuffer has no texture targets
	}


	void RenderManager::Shutdown()
	{
		LOG("Render manager shutting down...");
	}


	void RenderManager::Update()
	{
		Render();
	}


	void RenderManager::Render()
	{
		// TODO: This should be an API-agnostic function bound at runtime, to handle different submission techniques
		// between APIs
		// -> Just handle OpenGL for now...

		// TODO: Add an assert somewhere that checks if any possible shader uniform isn't set
		// -> Catch bugs where we forget to upload a common param

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

		// Display the final frame:
		m_context.SwapWindow();
	}


	void SaberEngine::RenderManager::Initialize()
	{
		// Add graphics systems, in order:
		m_graphicsSystems.emplace_back(make_shared<GBufferGraphicsSystem>("GBuffer Graphics System"));
		m_graphicsSystems.emplace_back(make_shared<ShadowsGraphicsSystem>("Shadows Graphics System"));
		m_graphicsSystems.emplace_back(make_shared<DeferredLightingGraphicsSystem>("Deferred Lighting Graphics System"));		
		m_graphicsSystems.emplace_back(make_shared<SkyboxGraphicsSystem>("Skybox Graphics System"));
		m_graphicsSystems.emplace_back(make_shared<BloomGraphicsSystem>("Bloom Graphics System"));
		m_graphicsSystems.emplace_back(make_shared<TonemappingGraphicsSystem>("Tonemapping Graphics System"));
		// NOTE: Adding a new graphics system? Don't forget to add a new template instantiation below GetGraphicsSystem()
		
		// Create each graphics system in turn:
		vector<shared_ptr<GraphicsSystem>>::iterator gsIt;
		for(gsIt = m_graphicsSystems.begin(); gsIt != m_graphicsSystems.end(); gsIt++)
		{
			(*gsIt)->Create(m_pipeline.AddNewStagePipeline((*gsIt)->GetName()));

			// If the GS didn't attach any render stages, remove it
			if (m_pipeline.GetPipeline().back().GetNumberOfStages() == 0)
			{
				m_pipeline.GetPipeline().pop_back();
				vector<shared_ptr<GraphicsSystem>>::iterator deleteIt = gsIt;
				gsIt--;
				m_graphicsSystems.erase(deleteIt);
			}
		}
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
	template std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem<SkyboxGraphicsSystem>();
	template std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem<BloomGraphicsSystem>();
	template std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem<TonemappingGraphicsSystem>();
}


