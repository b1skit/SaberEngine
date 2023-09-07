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
#include "ParameterBlock_OpenGL.h"
#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "Sampler_OpenGL.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Shader_OpenGL.h"
#include "SwapChain_OpenGL.h"
#include "TextureTarget.h"
#include "Transform.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"

using gr::BloomGraphicsSystem;
using gr::Camera;
using gr::DeferredLightingGraphicsSystem;
using gr::GBufferGraphicsSystem;
using gr::GraphicsSystem;
using re::RenderStage;
using gr::ShadowsGraphicsSystem;
using gr::SkyboxGraphicsSystem;
using re::StagePipeline;
using gr::TonemappingGraphicsSystem;
using gr::Transform;
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
		renderManager.m_graphicsSystems.emplace_back(make_shared<GBufferGraphicsSystem>("OpenGL GBuffer Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<ShadowsGraphicsSystem>("OpenGL Shadows Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<DeferredLightingGraphicsSystem>("OpenGL Deferred Lighting Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<SkyboxGraphicsSystem>("OpenGL Skybox Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<BloomGraphicsSystem>("OpenGL Bloom Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<TonemappingGraphicsSystem>("OpenGL Tonemapping Graphics System"));
	}


	void RenderManager::CreateAPIResources()
	{
		// Textures:
		if (!m_newTextures.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newTextures.m_mutex);
			for (auto& newObject : m_newTextures.m_newObjects)
			{
				opengl::Texture::Create(*newObject.second);
			}
			m_newTextures.m_newObjects.clear();
		}
		// Samplers:
		if (!m_newSamplers.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newSamplers.m_mutex);
			for (auto& newObject : m_newSamplers.m_newObjects)
			{
				opengl::Sampler::Create(*newObject.second);
			}
			m_newSamplers.m_newObjects.clear();
		}
		// Texture Target Sets:
		if (!m_newTargetSets.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newTargetSets.m_mutex);
			for (auto& newObject : m_newTargetSets.m_newObjects)
			{
				newObject.second->Commit();
				opengl::TextureTargetSet::CreateColorTargets(*newObject.second);
				opengl::TextureTargetSet::CreateDepthStencilTarget(*newObject.second);
			}
			m_newTargetSets.m_newObjects.clear();
		}
		// Shaders:
		if (!m_newShaders.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newShaders.m_mutex);
			for (auto& newObject : m_newShaders.m_newObjects)
			{
				opengl::Shader::Create(*newObject.second);
			}
			m_newShaders.m_newObjects.clear();
		}
		// Mesh Primitives:
		if (!m_newMeshPrimitives.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newMeshPrimitives.m_mutex);
			for (auto& newObject : m_newMeshPrimitives.m_newObjects)
			{
				opengl::MeshPrimitive::Create(*newObject.second);
			}
			m_newMeshPrimitives.m_newObjects.clear();
		}
		// Parameter Blocks:
		if (!m_newParameterBlocks.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newParameterBlocks.m_mutex);
			for (auto& newObject : m_newParameterBlocks.m_newObjects)
			{
				opengl::ParameterBlock::Create(*newObject.second);
			}
			m_newParameterBlocks.m_newObjects.clear();
		}
	}


	void RenderManager::Render()
	{
		opengl::Context* context = re::Context::GetAs<opengl::Context*>();

		// Render each stage:
		for (StagePipeline& stagePipeline : m_renderPipeline.GetStagePipeline())
		{
			// RenderDoc markers: Graphics system group name
			glPushDebugGroup(
				GL_DEBUG_SOURCE_APPLICATION, 
				0, 
				-1, 
				stagePipeline.GetName().c_str());

			// Generic lambda: Process stages from various pipelines
			auto ProcessRenderStage = [&](std::shared_ptr<re::RenderStage> renderStage)
			{
				// RenderDoc makers: Render stage name
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, renderStage->GetName().c_str());

				gr::PipelineState const& stagePipelineParams = renderStage->GetStagePipelineState();

				// Get the stage targets:
				std::shared_ptr<re::TextureTargetSet const> stageTargets = renderStage->GetTextureTargetSet();
				if (!stageTargets)
				{
					opengl::SwapChain::PlatformParams* swapChainParams = 
						context->GetSwapChain().GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();
					SEAssert("Swap chain params and backbuffer cannot be null", 
						swapChainParams && swapChainParams->m_backbufferTargetSet);

					stageTargets = swapChainParams->m_backbufferTargetSet; // Draw directly to the swapchain backbuffer
				}

				
				auto SetDrawState = [&renderStage, &context](
					re::Shader const* shader)
				{
					opengl::Shader::Bind(*shader);

					// Set stage param blocks:
					for (std::shared_ptr<re::ParameterBlock> permanentPB : renderStage->GetPermanentParameterBlocks())
					{
						opengl::Shader::SetParameterBlock(*shader, *permanentPB.get());
					}
					for (std::shared_ptr<re::ParameterBlock> perFramePB : renderStage->GetPerFrameParameterBlocks())
					{
						opengl::Shader::SetParameterBlock(*shader, *perFramePB.get());
					}

					// Set stage texture/sampler inputs:
					for (auto const& texSamplerInput : renderStage->GetTextureInputs())
					{
						opengl::Shader::SetTextureAndSampler(
							*shader,
							texSamplerInput.m_shaderName, // uniform name
							texSamplerInput.m_texture,
							texSamplerInput.m_sampler,
							texSamplerInput.m_subresource);
					}
				};

				// Bind the shader now that the pipeline state is set:
				re::Shader* stageShader = renderStage->GetStageShader();
				const bool hasStageShader = stageShader != nullptr;
				if (hasStageShader)
				{
					SetDrawState(stageShader);
				}

				// Configure the pipeline state:
				context->SetPipelineState(stagePipelineParams);

				switch (renderStage->GetStageType())
				{
				case re::RenderStage::RenderStageType::Compute:
				{
					opengl::TextureTargetSet::AttachTargetsAsImageTextures(*stageTargets);

					// TODO: Support compute target clearing
				}
				break;
				case re::RenderStage::RenderStageType::Graphics:
				{
					opengl::TextureTargetSet::AttachColorTargets(*stageTargets);
					opengl::TextureTargetSet::AttachDepthStencilTarget(*stageTargets);

					// Clear the targets AFTER setting color/depth write modes
					const gr::PipelineState::ClearTarget clearTargetMode = stagePipelineParams.GetClearTarget();
					if (clearTargetMode == gr::PipelineState::ClearTarget::Color ||
						clearTargetMode == gr::PipelineState::ClearTarget::ColorDepth)
					{
						opengl::TextureTargetSet::ClearColorTargets(*stageTargets);
					}
					if (clearTargetMode == gr::PipelineState::ClearTarget::Depth ||
						clearTargetMode == gr::PipelineState::ClearTarget::ColorDepth)
					{
						opengl::TextureTargetSet::ClearDepthStencilTarget(*stageTargets);
					}
				}
				break;
				default:
					SEAssertF("Invalid render stage type");
				}		

				// Render stage batches:
				std::vector<re::Batch> const& batches = renderStage->GetStageBatches();
				for (re::Batch const& batch : batches)
				{
					// No stage shader: Must set stage PBs for each batch
					if (!hasStageShader)
					{
						re::Shader const* batchShader = batch.GetShader();

						SetDrawState(batchShader);
					}

					// Batch parameter blocks:
					vector<shared_ptr<re::ParameterBlock>> const& batchPBs = batch.GetParameterBlocks();
					for (shared_ptr<re::ParameterBlock> batchPB : batchPBs)
					{
						opengl::Shader::SetParameterBlock(*stageShader, *batchPB.get());
					}

					// Set Batch Texture/Sampler inputs:
					if (stageTargets->WritesColor())
					{
						for (auto const& texSamplerInput : batch.GetTextureAndSamplerInputs())
						{
							opengl::Shader::SetTextureAndSampler(
								*stageShader,
								texSamplerInput.m_shaderName,
								texSamplerInput.m_texture,
								texSamplerInput.m_sampler,
								texSamplerInput.m_subresource);
						}
					}

					switch (renderStage->GetStageType())
					{
					case re::RenderStage::RenderStageType::Graphics:
					{
						opengl::MeshPrimitive::PlatformParams const* meshPlatParams =
							batch.GetMeshPrimitive()->GetPlatformParams()->As<opengl::MeshPrimitive::PlatformParams const*>();

						opengl::MeshPrimitive::Bind(*batch.GetMeshPrimitive());

						glDrawElementsInstanced(
							meshPlatParams->m_drawMode,			// GLenum mode
							(GLsizei)batch.GetMeshPrimitive()->GetVertexStream(re::MeshPrimitive::Indexes)->GetNumElements(),	// GLsizei count
							GL_UNSIGNED_INT,					// GLenum type. TODO: Store type in parameters, instead of assuming uints
							0,									// Byte offset (into bound index buffer)
							(GLsizei)batch.GetInstanceCount());	// Instance count
					}
					break;
					case re::RenderStage::RenderStageType::Compute:
					{
						glm::uvec3 const& threadGroupCount = batch.GetComputeParams().m_threadGroupCount;
						glDispatchCompute(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);

						// Barrier to prevent reading before texture writes have finished.
						// TODO: Is this always necessry? Should we be using different barrier types at any point?
						glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
					}
					break;
					default:
						SEAssertF("Invalid render stage type");
					}
					// Draw!
					
				} // batches

				glPopDebugGroup();
			}; // ProcessRenderStage


			// Process RenderStages:
			std::list<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
			for (std::shared_ptr<re::RenderStage> renderStage : renderStages)
			{			
				if (!renderStage->GetStageBatches().empty())
				{
					ProcessRenderStage(renderStage);
				}
			}

			glPopDebugGroup();
		}
	}


	void RenderManager::StartImGuiFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}


	void RenderManager::RenderImGui()
	{
		// Composite Imgui rendering on top of the finished frame:
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "ImGui stage");
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glPopDebugGroup();
	}


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
	}
}