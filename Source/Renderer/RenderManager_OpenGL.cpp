// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
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
		: gr::RenderManager(platform::RenderingAPI::OpenGL)
	{
	}


	void RenderManager::Initialize_Platform()
	{
		//
	}


	void RenderManager::BeginFrame_Platform(uint64_t frameNum)
	{
		//
	}


	void RenderManager::EndFrame_Platform()
	{
		//
	}


	void RenderManager::Render()
	{
		SEBeginCPUEvent("RenderManager::Render");

		opengl::Context* context = GetContext()->As<opengl::Context*>();

		re::GPUTimer& gpuTimer = context->GetGPUTimer();

		re::GPUTimer::Handle frameTimer = gpuTimer.StartTimer(nullptr, re::Context::k_GPUFrameTimerName);


		auto SetDrawState = [&context](
			gr::Stage const* stage,
			core::InvPtr<re::Shader> const& shader, 
			bool doSetStageInputs)
			{
				opengl::Shader::Bind(*shader);

				SEAssert(shader->GetRasterizationState() ||
					stage->GetStageType() == gr::Stage::Type::Compute,
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
			gr::RenderPipeline const& renderPipeline = renderSystem->GetRenderPipeline();

			re::GPUTimer::Handle renderPipelineTimer = 
				gpuTimer.StartTimer(nullptr, renderPipeline.GetName().c_str(), re::Context::k_GPUFrameTimerName);

			// Render each stage in the RenderSystem's RenderPipeline:			
			for (gr::StagePipeline const& stagePipeline : renderPipeline.GetStagePipeline())
			{
				re::GPUTimer::Handle stagePipelineTimer;
				bool isNewStagePipeline = true;			

				// Process Stages:
				std::list<std::shared_ptr<gr::Stage>> const& stages = stagePipeline.GetStages();
				for (std::shared_ptr<gr::Stage> const& stage : stages)
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

					const gr::Stage::Type curStageType = stage->GetStageType();
					switch (curStageType)
					{
					case gr::Stage::Type::LibraryRaster: // Library stages are executed with their own internal logic
					case gr::Stage::Type::LibraryCompute:
					{
						SEAssert(stage->GetRootConstants().GetRootConstantCount() == 0,
							"TODO: Handle setting root constants for library stages");

						dynamic_cast<gr::LibraryStage*>(stage.get())->Execute(nullptr);
					}
					break;
					case gr::Stage::Type::ClearTargetSet:
					{
						re::TextureTargetSet const* stageTargets = stage->GetTextureTargetSet();

						opengl::TextureTargetSet::AttachColorTargets(*stageTargets);
						opengl::TextureTargetSet::AttachDepthStencilTarget(*stageTargets);

						gr::ClearTargetSetStage const* clearStage = dynamic_cast<gr::ClearTargetSetStage const*>(stage.get());
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
					case gr::Stage::Type::ClearRWTextures:
					{
						gr::ClearRWTexturesStage const* clearStage =
							dynamic_cast<gr::ClearRWTexturesStage const*>(stage.get());
						SEAssert(clearStage, "Failed to get clear stage");

						void const* clearValue = clearStage->GetClearValue();
						switch (clearStage->GetClearValueType())
						{
						case gr::ClearRWTexturesStage::ValueType::Float:
						{
							opengl::TextureTargetSet::ClearImageTextures(clearStage->GetPermanentRWTextureInputs(),
								*static_cast<glm::vec4 const*>(clearValue));
							opengl::TextureTargetSet::ClearImageTextures(clearStage->GetSingleFrameRWTextureInputs(),
								*static_cast<glm::vec4 const*>(clearValue));
						}
						break;
						case gr::ClearRWTexturesStage::ValueType::Uint:
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
					case gr::Stage::Type::Copy:
					{
						gr::CopyStage const* copyStage = dynamic_cast<gr::CopyStage const*>(stage.get());
						SEAssert(copyStage, "Failed to get clear stage");

						opengl::TextureTargetSet::CopyTexture(copyStage->GetSrcTexture(), copyStage->GetDstTexture());
					}
					break;
					case gr::Stage::Type::Raster:
					case gr::Stage::Type::FullscreenQuad:
					case gr::Stage::Type::Compute:
					{
						// Get the stage targets:
						re::TextureTargetSet const* stageTargets = stage->GetTextureTargetSet();
						if (!stageTargets && curStageType != gr::Stage::Type::Compute)
						{
							// Draw to the swapchain backbuffer
							stageTargets = opengl::SwapChain::GetBackBufferTargetSet(context->GetSwapChain()).get();
						}
						SEAssert(stageTargets || curStageType == gr::Stage::Type::Compute,
							"The current stage does not have targets set. This is unexpected");

						switch (curStageType)
						{
						case gr::Stage::Type::Compute:
						{
							//
						}
						break;
						case gr::Stage::Type::Raster:
						case gr::Stage::Type::FullscreenQuad:
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
						std::vector<gr::StageBatchHandle> const& batches = stage->GetStageBatches();
						for (gr::StageBatchHandle const& batch : batches)
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
							std::vector<re::BufferInput> const& batchBuffers = (*batch)->GetBuffers();
							for (re::BufferInput const& batchBufferInput : batchBuffers)
							{
								opengl::Shader::SetBuffer(*currentShader, batchBufferInput);
							}

							// Single frame buffers:
							std::vector<re::BufferInput> const& singleFrameBuffers = batch.GetSingleFrameBuffers();
							for (re::BufferInput const& singleFrameBuffer : singleFrameBuffers)
							{
								opengl::Shader::SetBuffer(*currentShader, singleFrameBuffer);
							}

							// Set Batch Texture/Sampler inputs:
							for (auto const& texSamplerInput : (*batch)->GetTextureAndSamplerInputs())
							{
								opengl::Shader::SetTextureAndSampler(*currentShader, texSamplerInput);
							}

							// Batch compute inputs:
							opengl::Shader::SetImageTextureTargets(*currentShader, (*batch)->GetRWTextureInputs());

							// Batch root constants:
							opengl::Shader::SetRootConstants(*currentShader, (*batch)->GetRootConstants());							

							// Draw!
							switch (curStageType)
							{
							case gr::Stage::Type::Raster:
							case gr::Stage::Type::FullscreenQuad:
							{
								re::Batch::RasterParams const& rasterParams = (*batch)->GetRasterParams();
								
								// Set the VAO:
								// TODO: The VAO should be cached on the batch instead of re-hasing it for every single
								// batch
								const GLuint vertexStreamVAO = context->GetCreateVAO(
									batch,
									batch.GetIndexBuffer());
								if (vertexStreamVAO != currentVAO)
								{
									glBindVertexArray(vertexStreamVAO);
									currentVAO = vertexStreamVAO;
								}

								// Bind the vertex streams:
								for (uint8_t slotIdx = 0; slotIdx < re::VertexStream::k_maxVertexStreams; slotIdx++)
								{
									SEAssert(!batch.GetResolvedVertexBuffer(slotIdx).first ||
										(batch.GetResolvedVertexBuffer(slotIdx).first->GetStream() &&
											batch.GetResolvedVertexBuffer(slotIdx).second != re::VertexBufferInput::k_invalidSlotIdx),
										"Non-null VertexBufferInput pointer does not have a stream. This should not be possible");

									if (batch.GetResolvedVertexBuffer(slotIdx).first == nullptr)
									{
										break;
									}

									opengl::Buffer::Bind(
										*batch.GetResolvedVertexBuffer(slotIdx).first->GetBuffer(),
										opengl::Buffer::Vertex,
										batch.GetResolvedVertexBuffer(slotIdx).first->m_view,
										batch.GetResolvedVertexBuffer(slotIdx).second);
								}
								if (batch.GetIndexBuffer().GetStream())
								{
									opengl::Buffer::Bind(
										*batch.GetIndexBuffer().GetBuffer(),
										opengl::Buffer::Index,
										batch.GetIndexBuffer().m_view,
										0); // Arbitrary: Slot is not used for indexes
								}

								// Draw!
								switch (rasterParams.m_batchGeometryMode)
								{
								case re::Batch::GeometryMode::IndexedInstanced:
								{
									glDrawElementsInstanced(
										PrimitiveTopologyToGLPrimitiveType(rasterParams.m_primitiveTopology),	// GLenum mode
										static_cast<GLsizei>(batch.GetIndexBuffer().m_view.m_streamView.m_numElements),	// GLsizei count
										DataTypeToGLDataType(batch.GetIndexBuffer().m_view.m_streamView.m_dataType), 	// GLenum type
										0,									// Byte offset (into index buffer)
										(GLsizei)batch.GetInstanceCount());	// Instance count
								}
								break;
								case re::Batch::GeometryMode::ArrayInstanced:
								{
									const GLsizei numElements = static_cast<GLsizei>(
										batch.GetResolvedVertexBuffers()[0].first->m_view.m_streamView.m_numElements);

									glDrawArraysInstanced(
										PrimitiveTopologyToGLPrimitiveType(rasterParams.m_primitiveTopology),
										0,
										numElements,
										(GLsizei)batch.GetInstanceCount());
								}
								break;
								default: SEAssertF("Invalid batch geometry type");
								}
							}
							break;
							case gr::Stage::Type::Compute:
							{
								glm::uvec3 const& threadGroupCount = (*batch)->GetComputeParams().m_threadGroupCount;
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

		SEEndCPUEvent(); // "RenderManager::Render"
	}


	void RenderManager::Shutdown_Platform()
	{
		// Note: Shutdown order matters. Make sure any work performed here plays nicely with the 
		// gr::RenderManager::Shutdown ordering
	}
}