#include <vector>

#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "TextureTarget.h"
#include "Shader.h"
#include "Shader_OpenGL.h"
#include "Camera.h"
#include "Transform.h"
#include "GraphicsSystem.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"
#include "MeshPrimitive_OpenGL.h"

using re::RenderStage;
using re::TextureTargetSet;
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


		// Render each stage:
		for (StagePipeline& stagePipeline : renderManager.m_pipeline.GetPipeline())
		{
			// RenderDoc markers: Graphics system group name
			glPushDebugGroup(
				GL_DEBUG_SOURCE_APPLICATION, 
				0, 
				-1, 
				stagePipeline.GetName().c_str());

			// Generic lambda: Process stages from various pipelines
			auto ProcessRenderStage = [&](RenderStage* renderStage)
			{
				// RenderDoc makers: Render stage name
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, renderStage->GetName().c_str());

				RenderStage::PipelineStateParams const& stagePipelineParams = renderStage->GetStagePipelineStateParams();

				// Attach the stage targets:
				TextureTargetSet const& stageTargets = renderStage->GetTextureTargetSet();
				stageTargets.AttachColorDepthStencilTargets(
					stagePipelineParams.m_textureTargetSetConfig.m_targetFace, 
					stagePipelineParams.m_textureTargetSetConfig.m_targetMip, 
					true);
				
				// Configure the context:
				renderManager.m_context.SetCullingMode(stagePipelineParams.m_faceCullingMode);
				renderManager.m_context.SetBlendMode(stagePipelineParams.m_srcBlendMode, stagePipelineParams.m_dstBlendMode);
				renderManager.m_context.SetDepthTestMode(stagePipelineParams.m_depthTestMode);
				renderManager.m_context.SetDepthWriteMode(stagePipelineParams.m_depthWriteMode);
				renderManager.m_context.SetColorWriteMode(stagePipelineParams.m_colorWriteMode);
				renderManager.m_context.ClearTargets(stagePipelineParams.m_targetClearMode); // Clear AFTER setting color/depth modes
				// TODO: Move this to a "set pipeline state" helper within Context?

				// Bind the shader now that the context configuration is known:
				std::shared_ptr<gr::Shader> stageShader = renderStage->GetStageShader();
				opengl::Shader::Bind(*stageShader, true);
				// TODO: Handle shaders set by stages/materials/batches
				// Priority order: Stage, batch/material?

				// Set stage param blocks:
				opengl::Shader::SetParameterBlock(*stageShader, *stageTargets.GetTargetParameterBlock().get());

				for (std::shared_ptr<re::ParameterBlock> permanentPB : renderStage->GetPermanentParameterBlocks())
				{
					opengl::Shader::SetParameterBlock(*stageShader, *permanentPB.get());
				}
				for (std::shared_ptr<re::ParameterBlock> perFramePB : renderStage->GetPerFrameParameterBlocks())
				{
					opengl::Shader::SetParameterBlock(*stageShader, *perFramePB.get());
				}

				// Set per-frame stage shader uniforms:
				vector<RenderStage::StageShaderUniform> const& stagePerFrameShaderUniforms =
					renderStage->GetPerFrameShaderUniforms();
				for (RenderStage::StageShaderUniform curUniform : stagePerFrameShaderUniforms)
				{
					opengl::Shader::SetUniform(
						*stageShader, curUniform.m_uniformName, curUniform.m_value, curUniform.m_type, curUniform.m_count);
				}

				// Set camera params:
				Camera const* const stageCam = renderStage->GetStageCamera();
				if (stageCam)
				{
					opengl::Shader::SetParameterBlock(*stageShader, *stageCam->GetCameraParams().get());
				}

				// Render stage batches:
				std::vector<re::Batch> const& batches = renderStage->GetStageBatches();
				SEAssert("Stage does not have any batches to render", !batches.empty());
				for (re::Batch const& batch : batches)
				{
					opengl::MeshPrimitive::PlatformParams const* const meshPlatParams =
						dynamic_cast<opengl::MeshPrimitive::PlatformParams const* const>(batch.GetBatchMesh()->GetPlatformParams().get());

					opengl::MeshPrimitive::Bind(*batch.GetBatchMesh(), true);

					// Batch material:
					gr::Material* batchmaterial = batch.GetBatchMaterial();
					if (batchmaterial && renderStage->WritesColor())
					{
						// TODO: Reverse this logic. It should be stageShader->BindMaterial
						batchmaterial->BindToShader(stageShader);
					}

					// Batch parameter blocks:
					vector<shared_ptr<re::ParameterBlock>> const& batchPBs = batch.GetBatchParameterBlocks();
					for (shared_ptr<re::ParameterBlock> batchPB : batchPBs)
					{
						opengl::Shader::SetParameterBlock(*stageShader, *batchPB.get());
					}

					// Batch uniforms:
					for (re::Batch::ShaderUniform const& shaderUniform : batch.GetBatchUniforms())
					{
						opengl::Shader::SetUniform(*stageShader,
							shaderUniform.m_uniformName,
							shaderUniform.m_value.get(),
							shaderUniform.m_type,
							shaderUniform.m_count);
					}

					// Draw!
					glDrawElementsInstanced(
						meshPlatParams->m_drawMode,						// GLenum mode
						(GLsizei)batch.GetBatchMesh()->NumIndices(),	// GLsizei count
						GL_UNSIGNED_INT,								// GLenum type. TODO: Store type in parameters, instead of assuming uints
						0,												// Byte offset (into bound index buffer)
						(GLsizei)batch.GetInstanceCount());				// Instance count
				} // batches

				glPopDebugGroup();
			};


			// Single frame render stages:
			vector<RenderStage>& singleFrameRenderStages = stagePipeline.GetSingleFrameRenderStages();
			for (RenderStage& renderStage : singleFrameRenderStages)
			{
				ProcessRenderStage(&renderStage);
			}

			// Render stages:
			vector<RenderStage*> const& renderStages = stagePipeline.GetRenderStages();
			for (RenderStage* renderStage : renderStages)
			{			
				ProcessRenderStage(renderStage);
			}

			glPopDebugGroup();
		}



		// Cleanup:
		for (StagePipeline& stagePipeline : renderManager.m_pipeline.GetPipeline())
		{
			stagePipeline.EndOfFrame();
		}
	}	


	void RenderManager::RenderImGui(re::RenderManager& renderManager)
	{
		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// Process the queue of commands for the current frame:
		while (!renderManager.m_imGuiCommands.empty())
		{
			renderManager.m_imGuiCommands.front()->Execute();
			renderManager.m_imGuiCommands.pop();
		}

		// Composite Imgui rendering on top of the finished frame:
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "ImGui stage");
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glPopDebugGroup();
	}
}