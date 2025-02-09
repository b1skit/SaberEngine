// © 2022 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "Context_OpenGL.h"
#include "EnumTypes_OpenGL.h"
#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "Stage.h"
#include "RenderSystem.h"
#include "Sampler_OpenGL.h"
#include "Shader.h"
#include "Shader_OpenGL.h"
#include "SwapChain_OpenGL.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"

#include "Core/ProfilingMarkers.h"

#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...


namespace
{
	constexpr GLenum PrimitiveTopologyToGLPrimitiveType(gr::MeshPrimitive::PrimitiveTopology topologyMode)
	{
		switch (topologyMode)
		{
		case gr::MeshPrimitive::PrimitiveTopology::PointList: return GL_POINTS;
		case gr::MeshPrimitive::PrimitiveTopology::LineList: return GL_LINES;
		case gr::MeshPrimitive::PrimitiveTopology::LineStrip: return GL_LINE_STRIP;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleList: return GL_TRIANGLES;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
		case gr::MeshPrimitive::PrimitiveTopology::LineListAdjacency: return GL_LINES_ADJACENCY;
		case gr::MeshPrimitive::PrimitiveTopology::LineStripAdjacency: return GL_LINE_STRIP_ADJACENCY;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleListAdjacency: return GL_TRIANGLES_ADJACENCY;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleStripAdjacency: return GL_TRIANGLE_STRIP_ADJACENCY;
		default:
			SEAssertF("Unsupported topology mode");
			return GL_TRIANGLES;
		}
	}
}

namespace opengl
{
	RenderManager::RenderManager()
		: re::RenderManager(platform::RenderingAPI::OpenGL)
	{
	}


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		//
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Note: We've already obtained the read lock on all new resources by this point

		// Textures:
		if (renderManager.m_newTextures.HasReadData())
		{
			for (auto const& newObject : renderManager.m_newTextures.GetReadData())
			{
				opengl::Texture::Create(newObject);
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
			for (auto& vertexStream : renderManager.m_newVertexStreams.GetReadData())
			{
				vertexStream->CreateBuffers();
			}
		}
	}


	void RenderManager::BeginFrame(re::RenderManager&, uint64_t frameNum)
	{
		//
	}


	void RenderManager::EndFrame(re::RenderManager&)
	{
		//
	}


	void RenderManager::Render()
	{
		opengl::Context* context = re::Context::GetAs<opengl::Context*>();

		re::GPUTimer& gpuTimer = context->GetGPUTimer();

		re::GPUTimer::Handle frameTimer = gpuTimer.StartTimer(nullptr, k_GPUFrameTimerName);

		// Process RenderPiplines of each RenderSystem in turn:
		for (std::unique_ptr<gr::RenderSystem>& renderSystem : m_renderSystems)
		{
			re::RenderPipeline const& renderPipeline = renderSystem->GetRenderPipeline();

			re::GPUTimer::Handle renderPipelineTimer = 
				gpuTimer.StartTimer(nullptr, renderPipeline.GetName().c_str(), k_GPUFrameTimerName);

			// Render each stage in the RenderSystem's RenderPipeline:			
			for (re::StagePipeline const& stagePipeline : renderPipeline.GetStagePipeline())
			{
				re::GPUTimer::Handle stagePipelineTimer;
				bool isNewStagePipeline = true;			

				// Process Stages:
				std::list<std::shared_ptr<re::Stage>> const& stages = stagePipeline.GetStages();
				for (std::shared_ptr<re::Stage> stage : stages)
				{
					// Skip empty stages:
					if (stage->IsSkippable())
					{
						continue;
					}

					if (isNewStagePipeline)
					{
						SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsQueue, stagePipeline.GetName().c_str());
						stagePipelineTimer =
							gpuTimer.StartTimer(nullptr, stagePipeline.GetName().c_str(), renderPipeline.GetName().c_str());
						isNewStagePipeline = false;
					}

					// Profiling makers: Render stage name
					SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsQueue, stage->GetName().c_str());

					re::GPUTimer::Handle stageTimer =
						gpuTimer.StartTimer(nullptr, stage->GetName().c_str(), stagePipeline.GetName().c_str());

					// Library stages are executed with their own internal logic:
					const re::Stage::Type curStageType = stage->GetStageType();
					if (re::Stage::IsLibraryType(curStageType))
					{
						dynamic_cast<re::LibraryStage*>(stage.get())->Execute(nullptr);
						stageTimer.StopTimer(nullptr);
						SEEndOpenGLGPUEvent();
						continue;
					}

					// Get the stage targets:
					re::TextureTargetSet const* stageTargets = stage->GetTextureTargetSet();
					if (!stageTargets && curStageType != re::Stage::Type::Compute)
					{
						opengl::SwapChain::PlatformParams* swapChainParams =
							context->GetSwapChain().GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();
						SEAssert(swapChainParams && swapChainParams->m_backbufferTargetSet,
							"Swap chain params and backbuffer cannot be null");

						stageTargets = swapChainParams->m_backbufferTargetSet.get(); // Draw to the swapchain backbuffer
					}
					SEAssert(stageTargets || curStageType == re::Stage::Type::Compute,
						"The current stage does not have targets set. This is unexpected");


					auto SetDrawState = [&stage, &context](
						core::InvPtr<re::Shader> const& shader, bool doSetStageInputs)
					{
						opengl::Shader::Bind(*shader);

						SEAssert(shader->GetPipelineState() || 
							stage->GetStageType() == re::Stage::Type::Compute,
							"Pipeline state is null. This is unexpected");

						context->SetPipelineState(shader->GetPipelineState());

						if (doSetStageInputs)
						{
							// Set stage param blocks:
							for (re::BufferInput const& bufferInput : stage->GetPermanentBuffers())
							{
								opengl::Shader::SetBuffer(*shader, bufferInput);
							}
							for (re::BufferInput const& bufferInput : stage->GetPerFrameBuffers())
							{
								opengl::Shader::SetBuffer(*shader, bufferInput);
							}

							auto SetStageTextureInputs = [&shader](
								std::vector<re::TextureAndSamplerInput> const& texInputs)
								{
									for (auto const& texSamplerInput : texInputs)
									{
										opengl::Shader::SetTextureAndSampler(*shader, texSamplerInput);
									}
								};
							SetStageTextureInputs(stage->GetPermanentTextureInputs());
							SetStageTextureInputs(stage->GetSingleFrameTextureInputs());

							// Set compute inputs
							opengl::Shader::SetImageTextureTargets(*shader, stage->GetPermanentRWTextureInputs());
							opengl::Shader::SetImageTextureTargets(*shader, stage->GetSingleFrameRWTextureInputs());
						}
					};


					switch (curStageType)
					{
					case re::Stage::Type::Compute:
					{
						//
					}
					break;
					case re::Stage::Type::Graphics:
					case re::Stage::Type::FullscreenQuad:
					case re::Stage::Type::Clear:
					{
						opengl::TextureTargetSet::AttachColorTargets(*stageTargets);
						opengl::TextureTargetSet::AttachDepthStencilTarget(*stageTargets);

						opengl::TextureTargetSet::ClearTargets(*stageTargets);
					}
					break;
					default:
						SEAssertF("Invalid render stage type");
					}

					// OpenGL is stateful; We only need to set the stage inputs once
					bool hasSetStageInputs = false;

					core::InvPtr<re::Shader> currentShader;
					GLuint currentVAO = 0;

					// Stage batches:
					std::vector<re::Batch> const& batches = stage->GetStageBatches();
					for (re::Batch const& batch : batches)
					{
						core::InvPtr<re::Shader> const& batchShader = batch.GetShader();
						SEAssert(batchShader != nullptr, "Batch must have a shader");

						if (currentShader != batchShader)
						{
							currentShader = batchShader;

							SetDrawState(currentShader, !hasSetStageInputs);
							hasSetStageInputs = true;
						}
						SEAssert(currentShader, "Current shader is null");

						// Batch buffers:
						std::vector<re::BufferInput> const& batchBuffers = batch.GetBuffers();
						for (re::BufferInput const& batchBufferInput : batchBuffers)
						{
							opengl::Shader::SetBuffer(*currentShader, batchBufferInput);
						}

						// Set Batch Texture/Sampler inputs:
						for (auto const& texSamplerInput : batch.GetTextureAndSamplerInputs())
						{
							opengl::Shader::SetTextureAndSampler(*currentShader, texSamplerInput);
						}

						// Batch compute inputs:
						opengl::Shader::SetImageTextureTargets(*currentShader, batch.GetRWTextureInputs());

						// Draw!
						switch (curStageType)
						{
						case re::Stage::Type::Graphics:
						case re::Stage::Type::FullscreenQuad:
						{
							re::Batch::GraphicsParams const& batchGraphicsParams = batch.GetGraphicsParams();

							// Set the VAO:
							// TODO: The VAO should be cached on the batch instead of re-hasing it for every single
							// batch
							const GLuint vertexStreamVAO = context->GetCreateVAO(
								batchGraphicsParams.m_vertexBuffers, 
								batchGraphicsParams.m_indexBuffer);
							if (vertexStreamVAO != currentVAO)
							{
								glBindVertexArray(vertexStreamVAO);
								currentVAO = vertexStreamVAO;
							}

							// Bind the vertex streams:
							for (uint8_t slotIdx = 0; slotIdx < gr::VertexStream::k_maxVertexStreams; slotIdx++)
							{
								if (batchGraphicsParams.m_vertexBuffers[slotIdx].GetStream() == nullptr)
								{
									break;
								}

								opengl::Buffer::Bind(
									*batchGraphicsParams.m_vertexBuffers[slotIdx].GetBuffer(),
									opengl::Buffer::Vertex,
									batchGraphicsParams.m_vertexBuffers[slotIdx].m_view,
									batchGraphicsParams.m_vertexBuffers[slotIdx].m_bindSlot);
							}
							if (batchGraphicsParams.m_indexBuffer.GetStream())
							{
								opengl::Buffer::Bind(
									*batchGraphicsParams.m_indexBuffer.GetBuffer(),
									opengl::Buffer::Index,
									batchGraphicsParams.m_indexBuffer.m_view,
									0); // Arbitrary: Slot is not used for indexes
							}

							// Draw!
							switch (batchGraphicsParams.m_batchGeometryMode)
							{
							case re::Batch::GeometryMode::IndexedInstanced:
							{
								glDrawElementsInstanced(
									PrimitiveTopologyToGLPrimitiveType(batchGraphicsParams.m_primitiveTopology),			// GLenum mode
									static_cast<GLsizei>(batchGraphicsParams.m_indexBuffer.m_view.m_stream.m_numElements),	// GLsizei count
									DataTypeToGLDataType(batchGraphicsParams.m_indexBuffer.m_view.m_stream.m_dataType), 	// GLenum type
									0,									// Byte offset (into index buffer)
									(GLsizei)batch.GetInstanceCount());	// Instance count
							}
							break;
							case re::Batch::GeometryMode::ArrayInstanced:
							{							
								const GLsizei numElements = static_cast<GLsizei>(
									batchGraphicsParams.m_vertexBuffers[0].m_view.m_stream.m_numElements);

								glDrawArraysInstanced(
									PrimitiveTopologyToGLPrimitiveType(batchGraphicsParams.m_primitiveTopology),
									0,
									numElements,
									(GLsizei)batch.GetInstanceCount());
							}
							break;
							default: SEAssertF("Invalid batch geometry type");
							}
						}
						break;
						case re::Stage::Type::Compute:
						{
							glm::uvec3 const& threadGroupCount = batch.GetComputeParams().m_threadGroupCount;
							glDispatchCompute(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);

							// Barrier to prevent reading before texture writes have finished.
							// TODO: Is this always necessary?
							glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

							// TODO: I suspect we'll need this when sharing SSBOs between stages
							//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
						}
						break;
						default:
							SEAssertF("Invalid render stage type");
						}

					} // batches

					SEEndOpenGLGPUEvent();

					stageTimer.StopTimer(nullptr);
				}; // Stage loop

				if (!isNewStagePipeline) // Must have started a timer...
				{
					stagePipelineTimer.StopTimer(nullptr);
					SEEndOpenGLGPUEvent(); // StagePipeline
				}
			} // StagePipeline loop

			renderPipelineTimer.StopTimer(nullptr);
		} // m_renderSystems loop
	
		frameTimer.StopTimer(nullptr);

		gpuTimer.EndFrame();
	}


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
		// Note: Shutdown order matters. Make sure any work performed here plays nicely with the 
		// re::RenderManager::Shutdown ordering
	}
}