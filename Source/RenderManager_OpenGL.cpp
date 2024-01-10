// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

#include "Context_OpenGL.h"
#include "GraphicsSystem.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Culling.h"
#include "GraphicsSystem_Debug.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Tonemapping.h"
#include "ParameterBlock_OpenGL.h"
#include "ProfilingMarkers.h"
#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "RenderSystem.h"
#include "Sampler_OpenGL.h"
#include "Shader.h"
#include "Shader_OpenGL.h"
#include "SwapChain_OpenGL.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"
#include "VertexStream_OpenGL.h"


namespace
{
	constexpr GLenum TranslateToOpenGLPrimitiveType(gr::MeshPrimitive::TopologyMode topologyMode)
	{
		switch (topologyMode)
		{
		case gr::MeshPrimitive::TopologyMode::PointList: return GL_POINTS;
		case gr::MeshPrimitive::TopologyMode::LineList: return GL_LINES;
		case gr::MeshPrimitive::TopologyMode::LineStrip: return GL_LINE_STRIP;
		case gr::MeshPrimitive::TopologyMode::TriangleList: return GL_TRIANGLES;
		case gr::MeshPrimitive::TopologyMode::TriangleStrip: return GL_TRIANGLE_STRIP;
		case gr::MeshPrimitive::TopologyMode::LineListAdjacency: return GL_LINES_ADJACENCY;
		case gr::MeshPrimitive::TopologyMode::LineStripAdjacency: return GL_LINE_STRIP_ADJACENCY;
		case gr::MeshPrimitive::TopologyMode::TriangleListAdjacency: return GL_TRIANGLES_ADJACENCY;
		case gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency: return GL_TRIANGLE_STRIP_ADJACENCY;
		default:
			SEAssertF("Unsupported topology mode");
			return GL_TRIANGLES;
		}
	}


	constexpr GLenum TranslateToOpenGLDataType(re::VertexStream::DataType dataType)
	{
		switch (dataType)
		{
		case re::VertexStream::DataType::Float: return GL_FLOAT;
		case re::VertexStream::DataType::UInt: return GL_UNSIGNED_INT;
		case re::VertexStream::DataType::UShort: return GL_SHORT;
		case re::VertexStream::DataType::UByte: return GL_UNSIGNED_BYTE;
		default: SEAssertF("Unsupported data type");
			return GL_FLOAT;
		}
	}
}

namespace opengl
{
	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		renderManager.m_renderSystems.emplace_back(re::RenderSystem::Create("Default OpenGL RenderSystem"));
		re::RenderSystem* defaultRenderSystem = renderManager.m_renderSystems.back().get();

		// Build the initialization pipeline:
		auto InitializePipeline = [](re::RenderSystem* defaultRS)
		{
			gr::GraphicsSystemManager& gsm = defaultRS->GetGraphicsSystemManager();

			std::vector<std::shared_ptr<gr::GraphicsSystem>>& graphicsSystems = gsm.GetGraphicsSystems();

			// Create and add graphics systems:
			graphicsSystems.emplace_back(std::make_shared<gr::CullingGraphicsSystem>(&gsm));

			std::shared_ptr<gr::GBufferGraphicsSystem> gbufferGS = std::make_shared<gr::GBufferGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(gbufferGS);

			std::shared_ptr<gr::ShadowsGraphicsSystem> shadowGS = std::make_shared<gr::ShadowsGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(shadowGS);

			std::shared_ptr<gr::DeferredLightingGraphicsSystem> deferredLightingGS =
				std::make_shared<gr::DeferredLightingGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(deferredLightingGS);

			std::shared_ptr<gr::SkyboxGraphicsSystem> skyboxGS = std::make_shared<gr::SkyboxGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(skyboxGS);

			std::shared_ptr<gr::BloomGraphicsSystem> bloomGS = std::make_shared<gr::BloomGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(bloomGS);

			std::shared_ptr<gr::TonemappingGraphicsSystem> tonemappingGS = std::make_shared<gr::TonemappingGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(tonemappingGS);

			std::shared_ptr<gr::DebugGraphicsSystem> debugGS = std::make_shared<gr::DebugGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(debugGS);
		};
		defaultRenderSystem->SetInitializePipeline(InitializePipeline);


		// Build the create pipeline:
		auto CreatePipeline = [](re::RenderSystem* defaultRS)
		{
			gr::GraphicsSystemManager& gsm = defaultRS->GetGraphicsSystemManager();

			// Get our GraphicsSystems:
			gr::CullingGraphicsSystem* cullingGS = gsm.GetGraphicsSystem<gr::CullingGraphicsSystem>();
			gr::GBufferGraphicsSystem* gbufferGS = gsm.GetGraphicsSystem<gr::GBufferGraphicsSystem>();
			gr::ShadowsGraphicsSystem* shadowGS = gsm.GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
			gr::DeferredLightingGraphicsSystem* deferredLightingGS = gsm.GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();
			gr::SkyboxGraphicsSystem* skyboxGS = gsm.GetGraphicsSystem<gr::SkyboxGraphicsSystem>();
			gr::BloomGraphicsSystem* bloomGS = gsm.GetGraphicsSystem<gr::BloomGraphicsSystem>();
			gr::TonemappingGraphicsSystem* tonemappingGS = gsm.GetGraphicsSystem<gr::TonemappingGraphicsSystem>();
			gr::DebugGraphicsSystem* debugGS = gsm.GetGraphicsSystem<gr::DebugGraphicsSystem>();

			// Build the creation pipeline:
			gsm.Create();

			cullingGS->Create();
			deferredLightingGS->CreateResourceGenerationStages(
				defaultRS->GetRenderPipeline().AddNewStagePipeline("Deferred Lighting Resource Creation"));
			gbufferGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(gbufferGS->GetName()));
			shadowGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(shadowGS->GetName()));
			deferredLightingGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(deferredLightingGS->GetName()));
			skyboxGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(skyboxGS->GetName()));
			bloomGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(bloomGS->GetName()));
			tonemappingGS->Create(*defaultRS, defaultRS->GetRenderPipeline().AddNewStagePipeline(tonemappingGS->GetName()));
			debugGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(debugGS->GetName()));
		};
		defaultRenderSystem->SetCreatePipeline(CreatePipeline);


		// Build the update pipeline:
		auto UpdatePipeline = [](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();

			// Get our GraphicsSystems:
			gr::CullingGraphicsSystem* cullingGS = gsm.GetGraphicsSystem<gr::CullingGraphicsSystem>();
			gr::GBufferGraphicsSystem* gbufferGS = gsm.GetGraphicsSystem<gr::GBufferGraphicsSystem>();
			gr::ShadowsGraphicsSystem* shadowGS = gsm.GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
			gr::DeferredLightingGraphicsSystem* deferredLightingGS = gsm.GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();
			gr::SkyboxGraphicsSystem* skyboxGS = gsm.GetGraphicsSystem<gr::SkyboxGraphicsSystem>();
			gr::BloomGraphicsSystem* bloomGS = gsm.GetGraphicsSystem<gr::BloomGraphicsSystem>();
			gr::TonemappingGraphicsSystem* tonemappingGS = gsm.GetGraphicsSystem<gr::TonemappingGraphicsSystem>();
			gr::DebugGraphicsSystem* debugGS = gsm.GetGraphicsSystem<gr::DebugGraphicsSystem>();

			// Execute per-frame updates:
			gsm.PreRender();

			cullingGS->PreRender();
			gbufferGS->PreRender();
			shadowGS->PreRender();
			deferredLightingGS->PreRender();
			skyboxGS->PreRender();
			bloomGS->PreRender();
			tonemappingGS->PreRender();
			debugGS->PreRender();
		};
		defaultRenderSystem->SetUpdatePipeline(UpdatePipeline);
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Note: We've already obtained the read lock on all new resources by this point

		// Textures:
		if (renderManager.m_newTextures.HasReadData())
		{
			for (auto const& newObject : renderManager.m_newTextures.GetReadData())
			{
				opengl::Texture::Create(*newObject);
			}
		}
		// Samplers:
		if (renderManager.m_newSamplers.HasReadData())
		{
			for (auto& newObject : renderManager.m_newSamplers.GetReadData())
			{
				opengl::Sampler::Create(*newObject);
			}
		}
		// Texture Target Sets:
		if (renderManager.m_newTargetSets.HasReadData())
		{
			for (auto& newObject : renderManager.m_newTargetSets.GetReadData())
			{
				newObject->Commit();
				opengl::TextureTargetSet::CreateColorTargets(*newObject);
				opengl::TextureTargetSet::CreateDepthStencilTarget(*newObject);
			}
		}
		// Shaders:
		if (renderManager.m_newShaders.HasReadData())
		{
			for (auto& newObject : renderManager.m_newShaders.GetReadData())
			{
				opengl::Shader::Create(*newObject);
			}
		}
		
		// Vertex streams:
		if (renderManager.m_newVertexStreams.HasReadData())
		{
			for (auto& newObject : renderManager.m_newVertexStreams.GetReadData())
			{
				opengl::VertexStream::Create(*newObject);
			}
		}

		// Parameter Blocks:
		if (renderManager.m_newParameterBlocks.HasReadData())
		{
			for (auto& newObject : renderManager.m_newParameterBlocks.GetReadData())
			{
				opengl::ParameterBlock::Create(*newObject);
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
			for (re::StagePipeline& stagePipeline : renderPipeline.GetStagePipeline())
			{
				// Profiling markers: Graphics system group name
				SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsQueue, stagePipeline.GetName().c_str());

				// Process RenderStages:
				std::list<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
				for (std::shared_ptr<re::RenderStage> renderStage : renderStages)
				{
					// Skip empty stages:
					if (renderStage->IsSkippable())
					{
						continue;
					}

					// Profiling makers: Render stage name
					SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsQueue, renderStage->GetName().c_str());

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

					auto SetDrawState = [&renderStage, &context](re::Shader const* shader)
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
								texSamplerInput.m_srcMip);
						}

						context->SetPipelineState(shader->GetPipelineState());
					};

					re::Shader* stageShader = renderStage->GetStageShader();
					const bool hasStageShader = stageShader != nullptr;
					if (hasStageShader)
					{
						SetDrawState(stageShader);
					}

					switch (renderStage->GetStageType())
					{
					case re::RenderStage::RenderStageType::Compute:
					{
						opengl::TextureTargetSet::AttachTargetsAsImageTextures(*stageTargets);
					}
					break;
					case re::RenderStage::RenderStageType::Clear:
					case re::RenderStage::RenderStageType::Graphics:
					{
						opengl::TextureTargetSet::AttachColorTargets(*stageTargets);
						opengl::TextureTargetSet::AttachDepthStencilTarget(*stageTargets);
					}
					break;
					default:
						SEAssertF("Invalid render stage type");
					}

					GLuint currentVAO = 0;

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
						std::vector<std::shared_ptr<re::ParameterBlock>> const& batchPBs = batch.GetParameterBlocks();
						for (std::shared_ptr<re::ParameterBlock> const& batchPB : batchPBs)
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
									texSamplerInput.m_srcMip);
							}
						}

						// Draw!
						switch (renderStage->GetStageType())
						{
						case re::RenderStage::RenderStageType::Graphics:
						{
							re::Batch::GraphicsParams const& batchGraphicsParams = batch.GetGraphicsParams();

							 const uint8_t vertexStreamCount = 
								static_cast<uint8_t>(batchGraphicsParams.m_vertexStreams.size());

							// Set the VAO:
							// TODO: The VAO should be cached on the batch instead of re-hasing it for every single
							// batch. Fix this once we have a batch allocator
							const GLuint vertexStreamVAO = context->GetCreateVAO(
								batchGraphicsParams.m_vertexStreams.data(), 
								vertexStreamCount,
								batchGraphicsParams.m_indexStream);
							if (vertexStreamVAO != currentVAO)
							{
								glBindVertexArray(vertexStreamVAO);
								currentVAO = vertexStreamVAO;
							}

							// Bind the vertex streams:
							for (uint8_t slotIdx = 0; slotIdx < vertexStreamCount; slotIdx++)
							{
								if (batchGraphicsParams.m_vertexStreams[slotIdx])
								{
									opengl::VertexStream::Bind(
										*batchGraphicsParams.m_vertexStreams[slotIdx],
										static_cast<gr::MeshPrimitive::Slot>(slotIdx));
								}
							}
							if (batchGraphicsParams.m_indexStream)
							{
								opengl::VertexStream::Bind(
									*batchGraphicsParams.m_indexStream,
									static_cast<gr::MeshPrimitive::Slot>(0)); // Arbitrary slot, not used for indexes
							}

							// Draw!
							switch (batchGraphicsParams.m_batchGeometryMode)
							{
							case re::Batch::GeometryMode::IndexedInstanced:
							{
								glDrawElementsInstanced(
									TranslateToOpenGLPrimitiveType(batchGraphicsParams.m_batchTopologyMode),	// GLenum mode
									(GLsizei)batchGraphicsParams.m_indexStream->GetNumElements(),				// GLsizei count
									TranslateToOpenGLDataType(batchGraphicsParams.m_indexStream->GetDataType()),// GLenum type
									0,									// Byte offset (into index buffer)
									(GLsizei)batch.GetInstanceCount());	// Instance count
							}
							break;
							case re::Batch::GeometryMode::ArrayInstanced:
							{
								glDrawArraysInstanced(
									TranslateToOpenGLPrimitiveType(batchGraphicsParams.m_batchTopologyMode),
									0,
									(GLsizei)batchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position]->GetNumElements(),
									(GLsizei)batch.GetInstanceCount());
							}
							break;
							default: SEAssertF("Invalid batch geometry type");
							}
						}
						break;
						case re::RenderStage::RenderStageType::Compute:
						{
							glm::uvec3 const& threadGroupCount = batch.GetComputeParams().m_threadGroupCount;
							glDispatchCompute(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);

							// Barrier to prevent reading before texture writes have finished.
							// TODO: Is this always necessary?
							glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
						}
						break;
						default:
							SEAssertF("Invalid render stage type");
						}

					} // batches

					SEEndOpenGLGPUEvent();
				}; // ProcessRenderStage

				SEEndOpenGLGPUEvent(); // Graphics system group name
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
		SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsCommandList, "ImGui stage");
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SEEndOpenGLGPUEvent();
	}


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
	}
}