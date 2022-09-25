#include <vector>

#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "TextureTarget.h"
#include "Shader.h"
#include "Camera.h"
#include "Transform.h"
#include "GraphicsSystem.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"
#include "Mesh_OpenGL.h"

using gr::RenderStage;
using gr::TextureTargetSet;
using gr::Shader;
using gr::Camera;
using gr::Transform;
using gr::GraphicsSystem;
using gr::GBufferGraphicsSystem;
using gr::ShadowsGraphicsSystem;
using gr::DeferredLightingGraphicsSystem;
using gr::SkyboxGraphicsSystem;
using gr::BloomGraphicsSystem;
using gr::TonemappingGraphicsSystem;
using re::StagePipeline;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::string;
using std::vector;
using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;


namespace opengl
{
	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// Add graphics systems, in order:
		renderManager.m_graphicsSystems.emplace_back(make_shared<GBufferGraphicsSystem>("GBuffer Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<ShadowsGraphicsSystem>("Shadows Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<DeferredLightingGraphicsSystem>("Deferred Lighting Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<SkyboxGraphicsSystem>("Skybox Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<BloomGraphicsSystem>("Bloom Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<TonemappingGraphicsSystem>("Tonemapping Graphics System"));
		// NOTE: Adding a new graphics system? Don't forget to add a new template instantiation below GetGraphicsSystem()

		// Create each graphics system in turn:
		vector<shared_ptr<GraphicsSystem>>::iterator gsIt;
		for (gsIt = renderManager.m_graphicsSystems.begin(); gsIt != renderManager.m_graphicsSystems.end(); gsIt++)
		{
			(*gsIt)->Create(renderManager.m_pipeline.AddNewStagePipeline((*gsIt)->GetName()));

			// Remove GS if it didn't attach any render stages (Ensuring indexes of m_pipeline & m_graphicsSystems match)
			if (renderManager.m_pipeline.GetPipeline().back().GetNumberOfStages() == 0)
			{
				renderManager.m_pipeline.GetPipeline().pop_back();
				vector<shared_ptr<GraphicsSystem>>::iterator deleteIt = gsIt;
				gsIt--;
				renderManager.m_graphicsSystems.erase(deleteIt);
			}
		}
	}


	void RenderManager::Render(re::RenderManager& renderManager)
	{
		// TODO: Add an assert somewhere that checks if any possible shader uniform isn't set
		// -> Catch bugs where we forget to upload a common param

		// Update the graphics systems:
		for (size_t gs = 0; gs < renderManager.m_graphicsSystems.size(); gs++)
		{
			renderManager.m_graphicsSystems[gs]->PreRender(renderManager.m_pipeline.GetPipeline()[gs]);
		}

		// Render each stage:
		for (StagePipeline const& stagePipeline : renderManager.m_pipeline.GetPipeline())
		{
			// RenderDoc markers: Graphics system group name
			glPushDebugGroup(
				GL_DEBUG_SOURCE_APPLICATION, 
				0, 
				-1, 
				stagePipeline.GetName().c_str());

			// Generic lambda: Process stages from various pipelines
			auto ProcessRenderStage = [&](RenderStage const* renderStage)
			{
				// RenderDoc makers: Render stage name
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, renderStage->GetName().c_str());

				RenderStage::RenderStageParams const& renderStageParams = renderStage->GetStageParams();

				// Attach the stage targets:
				TextureTargetSet const& stageTargets = renderStage->GetTextureTargetSet();
				stageTargets.AttachColorDepthStencilTargets(
					renderStageParams.m_textureTargetSetConfig.m_targetFace, 
					renderStageParams.m_textureTargetSetConfig.m_targetMip, 
					true);

				// Configure the shader:
				std::shared_ptr<Shader const> stageShader = renderStage->GetStageShader();
				stageShader->Bind(true);
				// TODO: Use shaders from materials in some cases? Set shaders/materials per batch, don't decide here

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
				if (stageCam)
				{
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
				}

				// Configure the context:
				renderManager.m_context.SetCullingMode(renderStageParams.m_faceCullingMode);
				renderManager.m_context.SetBlendMode(renderStageParams.m_srcBlendMode, renderStageParams.m_dstBlendMode);
				renderManager.m_context.SetDepthTestMode(renderStageParams.m_depthTestMode);
				renderManager.m_context.SetDepthWriteMode(renderStageParams.m_depthWriteMode);
				renderManager.m_context.SetColorWriteMode(renderStageParams.m_colorWriteMode);
				renderManager.m_context.ClearTargets(renderStageParams.m_targetClearMode); // Clear AFTER setting color/depth modes
				// TODO: Move this to a "set pipeline state" helper within Context?

				// Render stage geometry:
				std::vector<std::shared_ptr<gr::Mesh>> const* meshes = renderStage->GetGeometryBatches();
				SEAssert("Stage does not have any geometry to render", meshes != nullptr);
				size_t meshIdx = 0;
				for (std::shared_ptr<gr::Mesh> mesh : *meshes)
				{
					mesh->Bind(true);

					shared_ptr<gr::Material> meshMaterial = mesh->MeshMaterial();
					if (meshMaterial != nullptr &&
						renderStage->WritesColor())
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
					const mat4 model = mesh->GetTransform().GetWorldMatrix();
					const mat4 modelRotation = mesh->GetTransform().GetWorldMatrix(Transform::Rotation);
					stageShader->SetUniform("in_model", &model[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					stageShader->SetUniform(
						"in_modelRotation", &modelRotation[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					// ^^ TODO: in_modelRotation isn't always used...

					if (stageCam)
					{
						const mat4 mv = stageCam->GetViewMatrix() * model;
						const mat4 mvp = stageCam->GetViewProjectionMatrix() * model;
						stageShader->SetUniform("in_mv", &mv[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
						stageShader->SetUniform("in_mvp", &mvp[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					}				
					// TODO: Figure out a more general solution for this?

					opengl::Mesh::PlatformParams const* const meshPlatParams=
						dynamic_cast<opengl::Mesh::PlatformParams const* const>(mesh->GetPlatformParams().get());

					// Draw!
					glDrawElements(
						meshPlatParams->m_drawMode,
						(GLsizei)mesh->NumIndices(),
						GL_UNSIGNED_INT, // TODO: Configure based on the Mesh/verts parameters, instead of assuming uints
						0); // (GLenum mode, GLsizei count, GLenum type, byte offset (to bound index buffer));

					meshIdx++;
				} // meshes

				glPopDebugGroup();
			};


			// Single frame render stages:
			vector<RenderStage> const& singleFrameRenderStages = stagePipeline.GetSingleFrameRenderStages();
			for (RenderStage const& renderStage : singleFrameRenderStages)
			{
				ProcessRenderStage(&renderStage);
			}

			// Render stages:
			vector<RenderStage const*> const& renderStages = stagePipeline.GetRenderStages();
			for (RenderStage const* renderStage : renderStages)
			{			
				ProcessRenderStage(renderStage);
			}

			glPopDebugGroup();
		}

		// Display the final frame:
		renderManager.m_context.SwapWindow();

		// Cleanup:
		for (StagePipeline& stagePipeline : renderManager.m_pipeline.GetPipeline())
		{
			stagePipeline.EndOfFrame();
		}
	}		
}