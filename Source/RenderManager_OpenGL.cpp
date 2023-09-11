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
#include "RenderSystem.h"
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
		renderManager.m_renderSystems.emplace_back(re::RenderSystem::Create("Default OpenGL RenderSystem"));
		re::RenderSystem* defaultRenderSystem = renderManager.m_renderSystems.back().get();	

		// Build the create pipeline:
		auto CreatePipeline = [](re::RenderSystem* defaultRS)
		{
			std::vector<std::shared_ptr<gr::GraphicsSystem>>& graphicsSystems = defaultRS->GetGraphicsSystems();

			// Create and add graphics systems:
			std::shared_ptr<gr::GBufferGraphicsSystem> gbufferGS = 
				std::make_shared<gr::GBufferGraphicsSystem>("OpenGL GBuffer Graphics System");
			graphicsSystems.emplace_back(gbufferGS);

			std::shared_ptr<gr::ShadowsGraphicsSystem> shadowGS =
				std::make_shared<gr::ShadowsGraphicsSystem>("OpenGL Shadows Graphics System");
			graphicsSystems.emplace_back(shadowGS);

			std::shared_ptr<gr::DeferredLightingGraphicsSystem> deferredLightingGS =
				std::make_shared<gr::DeferredLightingGraphicsSystem>("OpenGL Deferred Lighting Graphics System");
			graphicsSystems.emplace_back(deferredLightingGS);

			std::shared_ptr<gr::SkyboxGraphicsSystem> skyboxGS =
				std::make_shared<gr::SkyboxGraphicsSystem>("OpenGL Skybox Graphics System");
			graphicsSystems.emplace_back(skyboxGS);

			std::shared_ptr<gr::BloomGraphicsSystem> bloomGS =
				std::make_shared<gr::BloomGraphicsSystem>("OpenGL Bloom Graphics System");
			graphicsSystems.emplace_back(bloomGS);

			std::shared_ptr<gr::TonemappingGraphicsSystem> tonemappingGS =
				std::make_shared<gr::TonemappingGraphicsSystem>("OpenGL Tonemapping Graphics System");
			graphicsSystems.emplace_back(tonemappingGS);

			// Build the creation pipeline:
			deferredLightingGS->CreateResourceGenerationStages(
				defaultRS->GetRenderPipeline().AddNewStagePipeline("Deferred Lighting Resource Creation"));
			gbufferGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(gbufferGS->GetName()));
			shadowGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(shadowGS->GetName()));
			deferredLightingGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(deferredLightingGS->GetName()));
			skyboxGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(skyboxGS->GetName()));
			bloomGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(bloomGS->GetName()));
			tonemappingGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(tonemappingGS->GetName()));
		};
		defaultRenderSystem->SetCreatePipeline(CreatePipeline);


		// Build the update pipeline:
		auto UpdatePipeline = [](re::RenderSystem* renderSystem)
		{
			// Get our GraphicsSystems:
			gr::GBufferGraphicsSystem* gbufferGS = renderSystem->GetGraphicsSystem<gr::GBufferGraphicsSystem>();
			gr::ShadowsGraphicsSystem* shadowGS = renderSystem->GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
			gr::DeferredLightingGraphicsSystem* deferredLightingGS = renderSystem->GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();
			gr::SkyboxGraphicsSystem* skyboxGS = renderSystem->GetGraphicsSystem<gr::SkyboxGraphicsSystem>();
			gr::BloomGraphicsSystem* bloomGS = renderSystem->GetGraphicsSystem<gr::BloomGraphicsSystem>();
			gr::TonemappingGraphicsSystem* tonemappingGS = renderSystem->GetGraphicsSystem<gr::TonemappingGraphicsSystem>();

			// Execute per-frame updates:
			gbufferGS->PreRender();
			shadowGS->PreRender();
			deferredLightingGS->PreRender();
			skyboxGS->PreRender();
			bloomGS->PreRender();
			tonemappingGS->PreRender();
		};
		defaultRenderSystem->SetUpdatePipeline(UpdatePipeline);
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Note: We've already obtained the read lock on all new resources by this point

		// Textures:
		if (renderManager.m_newTextures.HasReadData())
		{
			for (auto& newObject : renderManager.m_newTextures.Get())
			{
				opengl::Texture::Create(*newObject.second);
			}
		}
		// Samplers:
		if (renderManager.m_newSamplers.HasReadData())
		{
			for (auto& newObject : renderManager.m_newSamplers.Get())
			{
				opengl::Sampler::Create(*newObject.second);
			}
		}
		// Texture Target Sets:
		if (renderManager.m_newTargetSets.HasReadData())
		{
			for (auto& newObject : renderManager.m_newTargetSets.Get())
			{
				newObject.second->Commit();
				opengl::TextureTargetSet::CreateColorTargets(*newObject.second);
				opengl::TextureTargetSet::CreateDepthStencilTarget(*newObject.second);
			}
		}
		// Shaders:
		if (renderManager.m_newShaders.HasReadData())
		{
			for (auto& newObject : renderManager.m_newShaders.Get())
			{
				opengl::Shader::Create(*newObject.second);
			}
		}
		// Mesh Primitives:
		if (renderManager.m_newMeshPrimitives.HasReadData())
		{
			for (auto& newObject : renderManager.m_newMeshPrimitives.Get())
			{
				opengl::MeshPrimitive::Create(*newObject.second);
			}
		}
		// Parameter Blocks:
		if (renderManager.m_newParameterBlocks.HasReadData())
		{
			for (auto& newObject : renderManager.m_newParameterBlocks.Get())
			{
				opengl::ParameterBlock::Create(*newObject.second);
			}
		}
	}


	void RenderManager::Render()
	{
		opengl::Context* context = re::Context::GetAs<opengl::Context*>();

		// Render each RenderSystem in turn:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			// Render each stage in the RenderSystem's RenderPipeline:
			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();
			for (StagePipeline& stagePipeline : renderPipeline.GetStagePipeline())
			{
				// RenderDoc markers: Graphics system group name
				glPushDebugGroup(
					GL_DEBUG_SOURCE_APPLICATION,
					0,
					-1,
					stagePipeline.GetName().c_str());

				// Process RenderStages:
				std::list<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
				for (std::shared_ptr<re::RenderStage> renderStage : renderStages)
				{
					// Skip empty stages:
					if (renderStage->GetStageBatches().empty())
					{
						continue;
					}

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

				glPopDebugGroup(); // Graphics system group name
			}
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