// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

#include "Camera.h"
#include "Context_OpenGL.h"
#include "GraphicsSystem.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"
#include "MeshPrimitive_OpenGL.h"
#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "Shader.h"
#include "Shader_OpenGL.h"
#include "SwapChain_OpenGL.h"
#include "TextureTarget.h"
#include "Transform.h"
#include "TextureTarget_OpenGL.h"
#include "SceneManager.h"

using re::RenderStage;
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
		// Add graphics systems, in order of execution:
		renderManager.m_graphicsSystems.emplace_back(make_shared<GBufferGraphicsSystem>("GBuffer Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<ShadowsGraphicsSystem>("Shadows Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<DeferredLightingGraphicsSystem>("Deferred Lighting Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<SkyboxGraphicsSystem>("Skybox Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<BloomGraphicsSystem>("Bloom Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<TonemappingGraphicsSystem>("Tonemapping Graphics System"));
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

				gr::PipelineState const& stagePipelineParams = renderStage->GetStagePipelineState();

				// Attach the stage targets:
				std::shared_ptr<re::TextureTargetSet> stageTargets = renderStage->GetTextureTargetSet();
				if (!stageTargets)
				{
					opengl::SwapChain::PlatformParams* swapChainParams = 
						renderManager.GetContext().GetSwapChain().GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();
					SEAssert("Swap chain params and backbuffer cannot be null", 
						swapChainParams && swapChainParams->m_backbufferTargetSet);

					stageTargets = swapChainParams->m_backbufferTargetSet; // Draw directly to the swapchain backbuffer
				}

				opengl::TextureTargetSet::AttachColorTargets(*stageTargets,
					stagePipelineParams.GetTextureTargetSetConfig().m_targetFace,
					stagePipelineParams.GetTextureTargetSetConfig().m_targetMip);
				opengl::TextureTargetSet::AttachDepthStencilTarget(*stageTargets);
				
				// Configure the pipeline state:
				opengl::Context::SetPipelineState(renderManager.m_context, stagePipelineParams);

				// Bind the shader now that the pipeline state is set:
				std::shared_ptr<re::Shader> stageShader = renderStage->GetStageShader();
				opengl::Shader::Bind(*stageShader);
				// TODO: Handle shaders set by stages/materials/batches
				// Priority order: Stage, batch/material?

				// Set stage param blocks:
				opengl::Shader::SetParameterBlock(*stageShader, *stageTargets->GetTargetParameterBlock().get());

				for (std::shared_ptr<re::ParameterBlock> permanentPB : renderStage->GetPermanentParameterBlocks())
				{
					opengl::Shader::SetParameterBlock(*stageShader, *permanentPB.get());
				}
				for (std::shared_ptr<re::ParameterBlock> perFramePB : renderStage->GetPerFrameParameterBlocks())
				{
					opengl::Shader::SetParameterBlock(*stageShader, *perFramePB.get());
				}

				// Set per-frame stage textures/sampler inputs:
				for (auto const& texSamplerInput : renderStage->GetPerFrameTextureInputs())
				{
					opengl::Shader::SetTextureAndSampler(
						*stageShader, 
						std::get<0>(texSamplerInput), // uniform name
						std::get<1>(texSamplerInput), // texture
						std::get<2>(texSamplerInput)); // sampler
				}

				// Render stage batches:
				std::vector<re::Batch> const& batches = renderStage->GetStageBatches();
				for (re::Batch const& batch : batches)
				{
					opengl::MeshPrimitive::PlatformParams const* meshPlatParams = 
						batch.GetBatchMesh()->GetPlatformParams()->As<opengl::MeshPrimitive::PlatformParams const*>();

					opengl::MeshPrimitive::Bind(*batch.GetBatchMesh());

					// Batch Texture/Sampler inputs:
					if (renderStage->WritesColor())
					{
						// TODO: Reconsider this WritesColor() check once alpha clipping/blending is implemented

						for (auto const& texSamplerInput : batch.GetBatchTextureAndSamplerInputs())
						{
							opengl::Shader::SetTextureAndSampler(
								*stageShader,
								std::get<0>(texSamplerInput), // uniform name
								std::get<1>(texSamplerInput), // texture
								std::get<2>(texSamplerInput)); // sampler
						}
					}

					// Batch parameter blocks:
					vector<shared_ptr<re::ParameterBlock>> const& batchPBs = batch.GetBatchParameterBlocks();
					for (shared_ptr<re::ParameterBlock> batchPB : batchPBs)
					{
						opengl::Shader::SetParameterBlock(*stageShader, *batchPB.get());
					}

					// Draw!
					glDrawElementsInstanced(
						meshPlatParams->m_drawMode,						// GLenum mode
						(GLsizei)batch.GetBatchMesh()->GetVertexStream(re::MeshPrimitive::Indexes)->GetNumElements(),	// GLsizei count
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
	}	


	void RenderManager::RenderImGui(re::RenderManager& renderManager)
	{
		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
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


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Shaders:
		if (!renderManager.m_newShaders.empty())
		{
			std::lock_guard<std::mutex> lock(renderManager.m_newShadersMutex);
			for (auto& shader : renderManager.m_newShaders)
			{
				opengl::Shader::Create(*shader.second);
			}
			renderManager.m_newShaders.clear();
		}
	}
}