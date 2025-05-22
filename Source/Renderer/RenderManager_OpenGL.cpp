// © 2022 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "Context_OpenGL.h"
#include "EnumTypes_OpenGL.h"
#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "RootConstants.h"
#include "Stage.h"
#include "RenderSystem.h"
#include "Sampler_OpenGL.h"
#include "Shader.h"
#include "Shader_OpenGL.h"
#include "SwapChain_OpenGL.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_Platform.h"

#include "Core/ProfilingMarkers.h"

#include <GL/glew.h>


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
				platform::Texture::CreateAPIResource(newObject, nullptr);
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
				vertexStream->CreateBuffers(vertexStream);
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


		auto SetDrawState = [&context](
			re::Stage const* stage,
			core::InvPtr<re::Shader> const& shader, 
			bool doSetStageInputs)
			{
				opengl::Shader::Bind(*shader);

				SEAssert(shader->GetRasterizationState() ||
					stage->GetStageType() == re::Stage::Type::Compute,
					"Pipeline state is null. This is unexpected");

				context->SetRasterizationState(shader->GetRasterizationState());

				if (doSetStageInputs)
				{
					// Set stage buffers:
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

				// Set root constants:
				opengl::Shader::SetRootConstants(*shader, stage->GetRootConstants());
			};


		// Process RenderPiplines of each RenderSystem in turn:
		for (std::unique_ptr<gr::RenderSystem> const& renderSystem : m_renderSystems)
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
				for (std::shared_ptr<re::Stage> const& stage : stages)
				{
					// Skip empty stages:
					if (stage->IsSkippable())
					{
						continue;
					}

					if (isNewStagePipeline)
					{
						SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsQueue, stagePipeline.GetName().c_str());
						stagePipelineTimer = gpuTimer.StartTimer(
							nullptr, stagePipeline.GetName().c_str(), renderPipeline.GetName().c_str());
						isNewStagePipeline = false;
					}

					// Profiling makers: Render stage name
					SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsQueue, stage->GetName().c_str());

					re::GPUTimer::Handle stageTimer =
						gpuTimer.StartTimer(nullptr, stage->GetName().c_str(), stagePipeline.GetName().c_str());

					const re::Stage::Type curStageType = stage->GetStageType();
					switch (curStageType)
					{
					case re::Stage::Type::LibraryGraphics: // Library stages are executed with their own internal logic
					case re::Stage::Type::LibraryCompute:
					{
						SEAssert(stage->GetRootConstants().GetRootConstantCount() == 0,
							"TODO: Handle setting root constants for library stages");

						dynamic_cast<re::LibraryStage*>(stage.get())->Execute(nullptr);
					}
					break;
					case re::Stage::Type::ClearTargetSet:
					{
						re::TextureTargetSet const* stageTargets = stage->GetTextureTargetSet();

						opengl::TextureTargetSet::AttachColorTargets(*stageTargets);
						opengl::TextureTargetSet::AttachDepthStencilTarget(*stageTargets);

						re::ClearTargetSetStage const* clearStage = dynamic_cast<re::ClearTargetSetStage const*>(stage.get());
						SEAssert(clearStage, "Failed to get clear stage");

						opengl::TextureTargetSet::ClearTargets(
							clearStage->GetAllColorClearModes(),
							clearStage->GetAllColorClearValues(),
							clearStage->GetNumColorClearElements(),
							clearStage->DepthClearEnabled(),
							clearStage->GetDepthClearValue(),
							clearStage->StencilClearEnabled(),
							clearStage->GetStencilClearValue(),
							*stageTargets);
					}
					break;
					case re::Stage::Type::ClearRWTextures:
					{
						re::ClearRWTexturesStage const* clearStage =
							dynamic_cast<re::ClearRWTexturesStage const*>(stage.get());
						SEAssert(clearStage, "Failed to get clear stage");

						void const* clearValue = clearStage->GetClearValue();
						switch (clearStage->GetClearValueType())
						{
						case re::ClearRWTexturesStage::ValueType::Float:
						{
							opengl::TextureTargetSet::ClearImageTextures(clearStage->GetPermanentRWTextureInputs(),
								*static_cast<glm::vec4 const*>(clearValue));
							opengl::TextureTargetSet::ClearImageTextures(clearStage->GetSingleFrameRWTextureInputs(),
								*static_cast<glm::vec4 const*>(clearValue));
						}
						break;
						case re::ClearRWTexturesStage::ValueType::Uint:
						{
							opengl::TextureTargetSet::ClearImageTextures(clearStage->GetPermanentRWTextureInputs(),
								*static_cast<glm::uvec4 const*>(clearValue));
							opengl::TextureTargetSet::ClearImageTextures(clearStage->GetSingleFrameRWTextureInputs(),
								*static_cast<glm::uvec4 const*>(clearValue));
						}
						break;
						default: SEAssertF("Invalid clear value type");
						}
					}
					break;
					case re::Stage::Type::Copy:
					{
						re::CopyStage const* copyStage = dynamic_cast<re::CopyStage const*>(stage.get());
						SEAssert(copyStage, "Failed to get clear stage");

						opengl::TextureTargetSet::CopyTexture(copyStage->GetSrcTexture(), copyStage->GetDstTexture());
					}
					break;
					case re::Stage::Type::Graphics:
					case re::Stage::Type::FullscreenQuad:
					case re::Stage::Type::Compute:
					{
						// Get the stage targets:
						re::TextureTargetSet const* stageTargets = stage->GetTextureTargetSet();
						if (!stageTargets && curStageType != re::Stage::Type::Compute)
						{
							// Draw to the swapchain backbuffer
							stageTargets = opengl::SwapChain::GetBackBufferTargetSet(context->GetSwapChain()).get();
						}
						SEAssert(stageTargets || curStageType == re::Stage::Type::Compute,
							"The current stage does not have targets set. This is unexpected");

						switch (curStageType)
						{
						case re::Stage::Type::Compute:
						{
							//
						}
						break;
						case re::Stage::Type::Graphics:
						case re::Stage::Type::FullscreenQuad:
						{
							opengl::TextureTargetSet::AttachColorTargets(*stageTargets);
							opengl::TextureTargetSet::AttachDepthStencilTarget(*stageTargets);
						}
						break;
						default: SEAssertF("Unexpected render stage type");
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

								SetDrawState(stage.get(), currentShader, !hasSetStageInputs);
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

							// Batch root constants:
							opengl::Shader::SetRootConstants(*currentShader, batch.GetRootConstants());							

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
										static_cast<GLsizei>(batchGraphicsParams.m_indexBuffer.m_view.m_streamView.m_numElements),	// GLsizei count
										DataTypeToGLDataType(batchGraphicsParams.m_indexBuffer.m_view.m_streamView.m_dataType), 	// GLenum type
										0,									// Byte offset (into index buffer)
										(GLsizei)batch.GetInstanceCount());	// Instance count
								}
								break;
								case re::Batch::GeometryMode::ArrayInstanced:
								{
									const GLsizei numElements = static_cast<GLsizei>(
										batchGraphicsParams.m_vertexBuffers[0].m_view.m_streamView.m_numElements);

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
								// TODO: Make this more granular: Use knowledge of future use to set required bits only
								glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

								// TODO: I suspect we'll need this when sharing SSBOs between stages
								//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
							}
							break;
							default:
								SEAssertF("Invalid render stage type");
							}

						} // batches
					}
					break;
					default: SEAssertF("Unexpected stage type");
					}

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