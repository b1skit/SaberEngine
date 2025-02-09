// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer_DX12.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "RenderManager_DX12.h"
#include "RenderSystem.h"
#include "Sampler_DX12.h"
#include "Shader_DX12.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

#include "Core/Assert.h"
#include "Core/PerfLogger.h"
#include "Core/ProfilingMarkers.h"

#include <d3dx12.h>

using Microsoft::WRL::ComPtr;


namespace dx12
{
	RenderManager::RenderManager()
		: re::RenderManager(platform::RenderingAPI::DX12)
		, m_numFrames(core::Config::Get()->GetValue<int>(core::configkeys::k_numBackbuffersKey))
	{
		SEAssert(m_numFrames >= 2 && m_numFrames <= 3, "Invalid number of frames in flight");
	}


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// Prepend DX12-specific render systems:
		renderManager.CreateAddRenderSystem("PlatformDX12", core::configkeys::k_platformPipelineFileName_DX12);
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Note: We've already obtained the read lock on all new resources by this point

		dx12::RenderManager& dx12RenderManager = dynamic_cast<dx12::RenderManager&>(renderManager);

		std::vector<std::future<void>> createTasks;

		static const bool singleThreadResourceCreate = 
			core::Config::Get()->KeyExists(core::configkeys::k_singleThreadGPUResourceCreation);

		// Textures:
		if (renderManager.m_newTextures.HasReadData())
		{
			auto CreateTextures = [&renderManager, singleThreaded = singleThreadResourceCreate]()
				{
					dx12::Context* context = re::Context::GetAs<dx12::Context*>();

					dx12::CommandQueue* copyQueue = &context->GetCommandQueue(dx12::CommandListType::Copy);

					SEBeginGPUEvent(copyQueue->GetD3DCommandQueue(), perfmarkers::Type::CopyQueue, "Copy Queue: Create API Resources");

					std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();

					re::GPUTimer::Handle texCopyTimer = context->GetGPUTimer().StartCopyTimer(
						copyCommandList->GetD3DCommandList(),
						"Copy textures",
						k_GPUFrameTimerName);

					if (!singleThreaded)
					{
						renderManager.m_newTextures.AquireReadLock();
					}
					for (auto& texture : renderManager.m_newTextures.GetReadData())
					{
						dx12::Texture::Create(texture, copyCommandList.get());
					}
					if (!singleThreaded)
					{
						renderManager.m_newTextures.ReleaseReadLock();
					}

					texCopyTimer.StopTimer(copyCommandList->GetD3DCommandList());

					copyQueue->Execute(1, &copyCommandList);

					SEEndGPUEvent(copyQueue->GetD3DCommandQueue());
				};
			
			if (singleThreadResourceCreate)
			{
				CreateTextures();
			}
			else
			{
				createTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(CreateTextures));
			}
		}

		// Samplers:
		if (renderManager.m_newSamplers.HasReadData())
		{
			auto CreateSamplers = [&renderManager, singleThreaded = singleThreadResourceCreate]()
				{
					if (!singleThreaded)
					{
						renderManager.m_newSamplers.AquireReadLock();
					}
					for (auto& newObject : renderManager.m_newSamplers.GetReadData())
					{
						dx12::Sampler::Create(*newObject);
					}
					if (!singleThreaded)
					{
						renderManager.m_newSamplers.ReleaseReadLock();
					}
				};

			if (singleThreadResourceCreate)
			{
				CreateSamplers();
			}
			else
			{
				createTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(CreateSamplers));
			}
			
		}
		// Texture Target Sets:
		if (renderManager.m_newTargetSets.HasReadData())
		{
			auto CreateTextureTargetSets = [&renderManager, singleThreaded = singleThreadResourceCreate]()
				{
					if (!singleThreaded)
					{
						renderManager.m_newTargetSets.AquireReadLock();
					}
					for (auto& newObject : renderManager.m_newTargetSets.GetReadData())
					{
						newObject->Commit();
						dx12::TextureTargetSet::CreateColorTargets(*newObject);
						dx12::TextureTargetSet::CreateDepthStencilTarget(*newObject);
					}
					if (!singleThreaded)
					{
						renderManager.m_newTargetSets.ReleaseReadLock();
					}
				};

			if (singleThreadResourceCreate)
			{
				CreateTextureTargetSets();
			}
			else
			{
				createTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(CreateTextureTargetSets));
			}
		}
		// Shaders:
		if (renderManager.m_newShaders.HasReadData())
		{
			auto CreateShaders = [&renderManager, singleThreaded = singleThreadResourceCreate]()
				{
					if (!singleThreaded)
					{
						renderManager.m_newShaders.AquireReadLock();
					}
					for (auto& shader : renderManager.m_newShaders.GetReadData())
					{
						dx12::Shader::Create(*shader);
					}
					if (!singleThreaded)
					{
						renderManager.m_newShaders.ReleaseReadLock();
					}
				};

			if (singleThreadResourceCreate)
			{
				CreateShaders();
			}
			else
			{
				createTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(CreateShaders));
			}
		}
		// Vertex streams:
		if (renderManager.m_newVertexStreams.HasReadData())
		{
			auto CreateVertexStreams = [&renderManager, singleThreaded = singleThreadResourceCreate]()
				{
					if (!singleThreaded)
					{
						renderManager.m_newVertexStreams.AquireReadLock();
					}
					for (auto& vertexStream : renderManager.m_newVertexStreams.GetReadData())
					{
						vertexStream->CreateBuffers();
					}
					if (!singleThreaded)
					{
						renderManager.m_newVertexStreams.ReleaseReadLock();
					}
				};

			if (singleThreadResourceCreate)
			{
				CreateVertexStreams();
			}
			else
			{
				createTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(CreateVertexStreams));
			}
		}

		// Finally, wait for everything to complete:
		if (!singleThreadResourceCreate)
		{
			for (auto& createTask : createTasks)
			{
				createTask.wait();
			}
		}
	}


	void RenderManager::BeginFrame(re::RenderManager&, uint64_t frameNum)
	{
		//
	}


	void RenderManager::EndFrame(re::RenderManager& renderManager)
	{
		SEBeginCPUEvent("dx12::RenderManager::EndFrame");

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		context->GetHeapManager().EndOfFrame(renderManager.m_renderFrameNum);

		SEEndCPUEvent();
	}


	void RenderManager::Render()
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);
		dx12::CommandQueue& computeQueue = context->GetCommandQueue(dx12::CommandListType::Compute);

		std::vector<std::future<std::shared_ptr<dx12::CommandList>>> commandListJobs;

		re::GPUTimer& gpuTimer = context->GetGPUTimer();
		re::GPUTimer::Handle frameTimer;


		auto RenderStageTypeToCommandListType = [](const re::Stage::Type stageType) -> dx12::CommandListType
			{
				switch (stageType)
				{
				case re::Stage::Type::Graphics:
				case re::Stage::Type::LibraryGraphics:
				case re::Stage::Type::FullscreenQuad:
				case re::Stage::Type::Clear: // All clears are currently done on the graphics queue
					return dx12::CommandListType::Direct;
				case re::Stage::Type::Compute:
				case re::Stage::Type::LibraryCompute:
					return dx12::CommandListType::Compute;
				case re::Stage::Type::Parent:
				case re::Stage::Type::Invalid:
				default: SEAssertF("Unexpected stage type");
				}
				return dx12::CommandListType_Invalid; // This should never happen
			};

		auto IsGraphicsQueueStageType = [&RenderStageTypeToCommandListType](const re::Stage::Type stageType) -> bool
			{
				return RenderStageTypeToCommandListType(stageType) == dx12::CommandListType::Direct;
			};

		auto CmdListTypeChanged = [&IsGraphicsQueueStageType](
			const re::Stage::Type prev, const re::Stage::Type current) -> bool
			{
				SEAssert(prev != re::Stage::Type::Parent &&
					prev != re::Stage::Type::Invalid,
					"Previous type should always represent the last command list executed");

				return current != re::Stage::Type::Parent &&
					IsGraphicsQueueStageType(prev) != IsGraphicsQueueStageType(current);
			};

		// A WorkRange spans a contiguous subset of the RenderStages within a single StagePipeline
		struct WorkRange
		{
			re::RenderPipeline const* m_renderPipeline;
			std::vector<re::StagePipeline>::const_iterator m_stagePipelineItr;
			std::list<std::shared_ptr<re::Stage>>::const_iterator m_renderStageBeginItr;
			std::list<std::shared_ptr<re::Stage>>::const_iterator m_renderStageEndItr;
		};


		auto RecordCommandList = [this, context, &RenderStageTypeToCommandListType](
			std::vector<WorkRange>&& workRangeIn,
			std::shared_ptr<dx12::CommandList>&& cmdListIn)
				-> std::shared_ptr<dx12::CommandList>
			{
				std::shared_ptr<dx12::CommandList> cmdList = std::move(cmdListIn);

				// We move the WorkRange here to ensure it is cleared even if we're recording single-threaded
				const std::vector<WorkRange> workRange = std::move(workRangeIn);

				SEAssert(!workRange.empty(), "Work range is empty");

				auto SetDrawState = [&context](
					re::Stage const* renderStage,
					re::Stage::Type stageType,
					core::InvPtr<re::Shader> const& shader,
					re::TextureTargetSet const* targetSet,
					dx12::CommandList* commandList,
					bool doSetStageInputsAndTargets)
					{
						// Set the pipeline state and root signature first:
						dx12::PipelineState const* pso = context->GetPipelineStateObject(*shader, targetSet);
						commandList->SetPipelineState(*pso);

						switch (stageType)
						{
						case re::Stage::Type::Graphics:
						case re::Stage::Type::FullscreenQuad:
						{
							commandList->SetGraphicsRootSignature(dx12::Shader::GetRootSignature(*shader));
						}
						break;
						case re::Stage::Type::Compute:
						{
							commandList->SetComputeRootSignature(dx12::Shader::GetRootSignature(*shader));
						}
						break;
						default: SEAssertF("Unexpected render stage type");
						}

						// Set buffers (Must happen after the root signature is set):
						for (re::BufferInput const& bufferInput : renderStage->GetPermanentBuffers())
						{
							commandList->SetBuffer(bufferInput);
						}
						for (re::BufferInput const& bufferInput : renderStage->GetPerFrameBuffers())
						{
							commandList->SetBuffer(bufferInput);
						}
						// TODO: We should pass the whole list of buffers to the command list, to allow resource
						// transitions to be processed in a single call

						// Set inputs and targets (once) now that the root signature is set
						if (doSetStageInputsAndTargets)
						{
							const int depthTargetTexInputIdx = renderStage->GetDepthTargetTextureInputIdx();

							auto SetStageTextureInputs = [&commandList, depthTargetTexInputIdx]
							(std::vector<re::TextureAndSamplerInput> const& texInputs)
								{
									for (size_t texIdx = 0; texIdx < texInputs.size(); texIdx++)
									{
										// If the depth target is read-only, and we've also used it as an input to the
										// stage, we skip the resource transition (it's handled when binding the depth
										// target as read only)
										const bool skipTransition = (texIdx == depthTargetTexInputIdx);

										commandList->SetTexture(texInputs[texIdx], skipTransition);
										// Note: Static samplers have already been set during root signature creation
									}
								};
							SetStageTextureInputs(renderStage->GetPermanentTextureInputs());
							SetStageTextureInputs(renderStage->GetSingleFrameTextureInputs());

							commandList->SetRWTextures(renderStage->GetPermanentRWTextureInputs());
							commandList->SetRWTextures(renderStage->GetSingleFrameRWTextureInputs());

							// Set the targets:
							switch (stageType)
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
								commandList->SetRenderTargets(*targetSet);
							}
							break;
							default:
								SEAssertF("Invalid stage type");
							}
						}
					};			


				// All stages in a range are recorded to the same queue/command list type
				
				const dx12::CommandListType cmdListType = cmdList->GetCommandListType();
				SEAssert(cmdListType == RenderStageTypeToCommandListType((*workRange[0].m_renderStageBeginItr)->GetStageType()),
					"Incorrect command list type received");

				perfmarkers::Type perfMarkerType = perfmarkers::Type::GraphicsCommandList;
				switch (cmdListType)
				{
				case dx12::CommandListType::Direct:
				{
					perfMarkerType = perfmarkers::Type::GraphicsCommandList;
				}
				break;
				case dx12::CommandListType::Compute:
				{
					perfMarkerType = perfmarkers::Type::ComputeCommandList;
				}
				break;
				default: SEAssertF("Unexpected command list type");
				}

				re::RenderPipeline const* lastSeenRenderPipeline = nullptr;
				re::StagePipeline const* lastSeenStagePipeline = nullptr;

				re::GPUTimer& gpuTimer = context->GetGPUTimer();
				re::GPUTimer::Handle renderPipelineTimer;
				re::GPUTimer::Handle stagePipelineTimer;

				// Process our WorkRanges:
				auto workRangeItr = workRange.begin();
				while (workRangeItr != workRange.end())
				{
					const bool isLastWorkEntry = std::next(workRangeItr) == workRange.end();

					re::RenderPipeline const* renderPipeline = workRangeItr->m_renderPipeline;
					const bool isNewRenderPipeline = lastSeenRenderPipeline != renderPipeline;
					if (isNewRenderPipeline)
					{
						lastSeenRenderPipeline = renderPipeline;

						renderPipelineTimer.StopTimer(cmdList->GetD3DCommandList());

						renderPipelineTimer = gpuTimer.StartTimer(cmdList->GetD3DCommandList(),
							renderPipeline->GetName().c_str(),
							k_GPUFrameTimerName);

						// We don't add a GPU event marker for render systems to minimize noise in captures
					}
					const bool isLastOfRenderSystem = isLastWorkEntry ||
						lastSeenRenderPipeline != std::next(workRangeItr)->m_renderPipeline;

					re::StagePipeline const& stagePipeline = (*workRangeItr->m_stagePipelineItr);

					const bool isNewStagePipeline = lastSeenStagePipeline != &(*workRangeItr->m_stagePipelineItr);
					if (isNewStagePipeline)
					{
						lastSeenStagePipeline = &(*workRangeItr->m_stagePipelineItr);

						stagePipelineTimer.StopTimer(cmdList->GetD3DCommandList());

						stagePipelineTimer = gpuTimer.StartTimer(cmdList->GetD3DCommandList(),
							stagePipeline.GetName().c_str(),
							renderPipeline->GetName().c_str());

						SEBeginGPUEvent( // StagePipeline
							cmdList->GetD3DCommandList(),
							perfMarkerType,
							stagePipeline.GetName().c_str());
					}
					const bool isLastOfStagePipeline = isLastWorkEntry ||
						lastSeenStagePipeline != &(*std::next(workRangeItr)->m_stagePipelineItr);
					
					// Stage ranges are contiguous within a single StagePipeline
					auto renderStageItr = workRangeItr->m_renderStageBeginItr;
					while (renderStageItr != workRangeItr->m_renderStageEndItr)
					{
						SEBeginGPUEvent( // Stage
							cmdList->GetD3DCommandList(),
							perfMarkerType,
							(*renderStageItr)->GetName().c_str());

						re::GPUTimer::Handle renderStageTimer = gpuTimer.StartTimer(cmdList->GetD3DCommandList(),
							(*renderStageItr)->GetName().c_str(),
							stagePipeline.GetName().c_str());

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
						cmdList->RecordStageName(renderStage->GetName());
#endif

						const re::Stage::Type curRenderStageType = (*renderStageItr)->GetStageType();
						if (re::Stage::IsLibraryType(curRenderStageType))
						{
							// Library stages are executed with their own internal logic
							dynamic_cast<re::LibraryStage*>((*renderStageItr).get())->Execute(cmdList.get());
						}
						else
						{
							// Get the stage targets:
							re::TextureTargetSet const* stageTargets = (*renderStageItr)->GetTextureTargetSet();
							if (stageTargets == nullptr && curRenderStageType != re::Stage::Type::Compute)
							{
								stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain()).get();
							}
							SEAssert(stageTargets || curRenderStageType == re::Stage::Type::Compute,
								"The current stage does not have targets set. This is unexpected");


							// Clear the targets
							switch (curRenderStageType)
							{
							case re::Stage::Type::Compute:
							case re::Stage::Type::LibraryCompute:
							{
								SEAssert(cmdList->GetCommandListType() == dx12::CommandListType::Compute,
									"Incorrect command list type");

								// TODO: Support compute target clearing
							}
							break;
							case re::Stage::Type::Graphics:
							case re::Stage::Type::LibraryGraphics:
							case re::Stage::Type::FullscreenQuad:
							case re::Stage::Type::Clear:
							{
								SEAssert(cmdList->GetCommandListType() == dx12::CommandListType::Direct,
									"Incorrect command list type");

								// Note: We do not have to have SetRenderTargets() to clear them in DX12
								cmdList->ClearTargets(*stageTargets);
							}
							break;
							default: SEAssertF("Invalid stage type");
							}

							core::InvPtr<re::Shader> currentShader;
							bool hasSetStageInputsAndTargets = false;

							// Stage batches:
							std::vector<re::Batch> const& batches = (*renderStageItr)->GetStageBatches();
							for (size_t batchIdx = 0; batchIdx < batches.size(); batchIdx++)
							{
								core::InvPtr<re::Shader> const& batchShader = batches[batchIdx].GetShader();
								SEAssert(batchShader != nullptr, "Batch must have a shader");

								if (currentShader != batchShader)
								{
									currentShader = batchShader;

									SetDrawState(
										(*renderStageItr).get(),
										curRenderStageType,
										currentShader,
										stageTargets,
										cmdList.get(),
										!hasSetStageInputsAndTargets);
									hasSetStageInputsAndTargets = true;
								}
								SEAssert(currentShader, "Current shader is null");

								// Batch buffers:
								std::vector<re::BufferInput> const& batchBuffers = batches[batchIdx].GetBuffers();
								for (size_t bufferIdx = 0; bufferIdx < batchBuffers.size(); ++bufferIdx)
								{
									cmdList->SetBuffer(batchBuffers[bufferIdx]);
								}

								// Batch Texture / Sampler inputs :
								for (auto const& texSamplerInput : batches[batchIdx].GetTextureAndSamplerInputs())
								{
									SEAssert(!stageTargets->HasDepthTarget() ||
										texSamplerInput.m_texture != stageTargets->GetDepthStencilTarget().GetTexture(),
										"We don't currently handle batches with the current depth buffer attached as "
										"a texture input. We need to make sure skipping transitions is handled correctly here");

									cmdList->SetTexture(texSamplerInput, false);
									// Note: Static samplers have already been set during root signature creation
								}

								// Batch compute inputs:
								cmdList->SetRWTextures(batches[batchIdx].GetRWTextureInputs());

								switch (curRenderStageType)
								{
								case re::Stage::Type::Graphics:
								case re::Stage::Type::FullscreenQuad:
								{
									SEAssert(cmdList->GetCommandListType() == dx12::CommandListType::Direct,
										"Incorrect command list type");

									cmdList->DrawBatchGeometry(batches[batchIdx]);
								}
								break;
								case re::Stage::Type::Compute:
								{
									SEAssert(cmdList->GetCommandListType() == dx12::CommandListType::Compute,
										"Incorrect command list type");

									cmdList->Dispatch(batches[batchIdx].GetComputeParams().m_threadGroupCount);
								}
								break;
								default: SEAssertF("Unexpected render stage type");
								}
							}
						}

						renderStageTimer.StopTimer(cmdList->GetD3DCommandList());
						SEEndGPUEvent(cmdList->GetD3DCommandList()); // Stage

						++renderStageItr;
					}

					if (isLastOfStagePipeline)
					{
						stagePipelineTimer.StopTimer(cmdList->GetD3DCommandList());
						SEEndGPUEvent(cmdList->GetD3DCommandList()); // StagePipeline
					}

					if (isLastOfRenderSystem)
					{
						renderPipelineTimer.StopTimer(cmdList->GetD3DCommandList());
						// No RenderSystem GPUEvent marker to end
					}
					
					++workRangeItr;
				}

				return cmdList;
			};

		auto EnqueueWorkRecording = 
			[&commandListJobs, &RecordCommandList, &RenderStageTypeToCommandListType, &directQueue, &computeQueue, &frameTimer]
			(std::vector<WorkRange>&& workRange, bool startGPUFrameTimer, bool stopGPUFrameTimer)
			{
				if (workRange.empty())
				{
					return;
				}

				const dx12::CommandListType cmdListType =
					RenderStageTypeToCommandListType((*workRange[0].m_renderStageBeginItr)->GetStageType());

				std::shared_ptr<dx12::CommandList> cmdList;
				switch (cmdListType)
				{
				case dx12::CommandListType::Direct:
				{
					cmdList = directQueue.GetCreateCommandList();
				}
				break;
				case dx12::CommandListType::Compute:
				{
					cmdList = computeQueue.GetCreateCommandList();
				}
				break;
				default: SEAssertF("Unexpected command list type");
				}

				if (startGPUFrameTimer)
				{
					frameTimer = re::Context::Get()->GetGPUTimer().StartTimer(
						cmdList->GetD3DCommandList(),
						k_GPUFrameTimerName);
				}

				static const bool s_recordSingleThreaded = 
					core::Config::Get()->KeyExists(core::configkeys::k_singleThreadCmdListRecording);
				if (s_recordSingleThreaded)
				{
					cmdList = RecordCommandList(std::move(workRange), std::move(cmdList));

					if (stopGPUFrameTimer)
					{
						frameTimer.StopTimer(cmdList->GetD3DCommandList());
					}

					switch (cmdListType)
					{
					case dx12::CommandListType::Direct:
					{
						directQueue.Execute(1, &cmdList);
					}
					break;
					case dx12::CommandListType::Compute:
					{
						computeQueue.Execute(1, &cmdList);
					}
					break;
					default: SEAssertF("Unexpected command list type");
					}
				}
				else
				{
					commandListJobs.emplace_back(core::ThreadPool::Get()->EnqueueJob(
						[workRange = std::move(workRange), 
							cmdList = std::move(cmdList),
							&RecordCommandList,
							&frameTimer,
							stopGPUFrameTimer]() mutable
						{
							std::shared_ptr<dx12::CommandList> populatedCmdList = 
								RecordCommandList(std::move(workRange), std::move(cmdList));

							if (stopGPUFrameTimer)
							{
								frameTimer.StopTimer(populatedCmdList->GetD3DCommandList());
							}

							return populatedCmdList;
						}));
				}
			};


		// Populate sets of WorkRanges that can be recorded on the same command list. A single WorkRange spans a
		// contiguous subset of the RenderStages of a single StagePipeline, we asyncronously record all work on a single 
		// command list and then immediately execute it when we detect the command list type has changed
		std::vector<WorkRange> workRange;

		re::Stage::Type prevRenderStageType = re::Stage::Type::Invalid;
		bool mustStartFrameTimer = true;

		auto renderSystemItr = m_renderSystems.begin();
		while (renderSystemItr != m_renderSystems.end())
		{
			re::RenderPipeline& renderPipeline = (*renderSystemItr)->GetRenderPipeline();

			auto stagePipelineItr = renderPipeline.GetStagePipeline().begin();
			while (stagePipelineItr != renderPipeline.GetStagePipeline().end())
			{
				std::list<std::shared_ptr<re::Stage>> const& renderStages = (*stagePipelineItr).GetRenderStages();
				if (renderStages.empty())
				{
					++stagePipelineItr;
					continue;
				}

				auto renderStageStartItr = renderStages.begin();
				auto renderStageEndItr = renderStages.begin();
				while (renderStageEndItr != renderStages.end())
				{
					// Skip empty stages:
					if ((*renderStageEndItr)->IsSkippable())
					{
						// If we've traversed beyond the 1st element, record some work:
						if (renderStageEndItr != renderStageStartItr)
						{
							workRange.emplace_back(WorkRange{
								.m_renderPipeline = &(*renderSystemItr)->GetRenderPipeline(),
								.m_stagePipelineItr = stagePipelineItr,
								.m_renderStageBeginItr = renderStageStartItr,
								.m_renderStageEndItr = renderStageEndItr, });
						}

						++renderStageEndItr; // This element is skipable: Advance before we update renderStageStartItr
						renderStageStartItr = renderStageEndItr;

						continue;
					}

					// We've found our first valid Stage: Initialize our state:
					if (prevRenderStageType == re::Stage::Type::Invalid)
					{
						prevRenderStageType = (*renderStageEndItr)->GetStageType();
						SEAssert(prevRenderStageType != re::Stage::Type::Invalid, "Invalid stage type");
					}

					const re::Stage::Type curRenderStageType = (*renderStageEndItr)->GetStageType();
					const bool cmdListTypeChanged = CmdListTypeChanged(prevRenderStageType, curRenderStageType);
					if (cmdListTypeChanged)
					{
						// If we've traversed beyond the 1st element, record some work:
						if (renderStageEndItr != renderStageStartItr)
						{
							workRange.emplace_back(WorkRange{
								.m_renderPipeline = &(*renderSystemItr)->GetRenderPipeline(),
								.m_stagePipelineItr = stagePipelineItr,
								.m_renderStageBeginItr = renderStageStartItr,
								.m_renderStageEndItr = renderStageEndItr, });
						}

						EnqueueWorkRecording(std::move(workRange), mustStartFrameTimer, false);
						mustStartFrameTimer = false;

						renderStageStartItr = renderStageEndItr;
						prevRenderStageType = curRenderStageType;
					}

					++renderStageEndItr;

					const bool isLastRenderStage = renderStageEndItr == renderStages.end();
					if (isLastRenderStage)
					{
						workRange.emplace_back(WorkRange{
							.m_renderPipeline = &(*renderSystemItr)->GetRenderPipeline(),
							.m_stagePipelineItr = stagePipelineItr,
							.m_renderStageBeginItr = renderStageStartItr,
							.m_renderStageEndItr = renderStages.end(), });
					}
				}
				++stagePipelineItr;
			}		
			++renderSystemItr;
		}

		// Enqueue any remaining work:
		SEAssert(!workRange.empty(), "No work to record: Frame timer won't be closed");
		EnqueueWorkRecording(std::move(workRange), false, true);

		// Submit asyncronously recorded command lists:
		for (auto& job : commandListJobs)
		{
			try
			{
				std::shared_ptr<dx12::CommandList> cmdList = job.get();

				switch (cmdList->GetCommandListType())
				{
				case dx12::CommandListType::Direct:
				{
					directQueue.Execute(1, &cmdList);
				}
				break;
				case dx12::CommandListType::Compute:
				{
					computeQueue.Execute(1, &cmdList);
				}
				break;
				default: SEAssertF("Unexpected command list type");
				}
			}
			catch (std::exception const& e)
			{
				SEAssertF(e.what());
			}
		}

		re::Context::Get()->GetGPUTimer().EndFrame();
	}


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
		// Note: Shutdown order matters. Make sure any work performed here plays nicely with the 
		// re::RenderManager::Shutdown ordering

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		
		for (size_t i = 0; i < dx12::CommandListType_Count; i++)
		{
			CommandQueue& commandQueue = context->GetCommandQueue(static_cast<dx12::CommandListType>(i));
			if (commandQueue.IsCreated())
			{
				context->GetCommandQueue(static_cast<dx12::CommandListType>(i)).Flush();
			}
		}
	}
}
