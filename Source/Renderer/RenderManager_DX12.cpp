// © 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure_DX12.h"
#include "AccelerationStructure_Platform.h"
#include "Batch.h"
#include "Context_DX12.h"
#include "RenderManager_DX12.h"
#include "RenderSystem.h"
#include "Sampler_DX12.h"
#include "Shader_DX12.h"
#include "ShaderBindingTable_DX12.h"
#include "Stage.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_Platform.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/ProfilingMarkers.h"


namespace dx12
{
	uint8_t RenderManager::s_numFrames_dx12 = 0;

	RenderManager::RenderManager()
		: re::RenderManager(platform::RenderingAPI::DX12)
		, m_numFrames(core::Config::Get()->GetValue<int>(core::configkeys::k_numBackbuffersKey))
	{
		SEAssert(m_numFrames >= 2 && m_numFrames <= 3, "Invalid number of frames in flight");
		s_numFrames_dx12 = m_numFrames;
	}


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// Prepend DX12-specific render systems:
		renderManager.CreateAddRenderSystem(core::configkeys::k_platformPipelineFileName_DX12);
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Note: We've already obtained the read lock on all new resources by this point

		dx12::RenderManager& dx12RenderManager = dynamic_cast<dx12::RenderManager&>(renderManager);

		constexpr size_t k_invalidCreateTaskIdx = std::numeric_limits<size_t>::max();
		constexpr size_t k_createTasksReserveAmt = 7;
		std::vector<std::shared_future<void>> createTasks;
		createTasks.reserve(k_createTasksReserveAmt);

		static const bool singleThreadResourceCreate = 
			core::Config::Get()->KeyExists(core::configkeys::k_singleThreadGPUResourceCreation);

		// Textures:
		if (renderManager.m_newTextures.HasReadData())
		{
			auto CreateTextures = [&renderManager, singleThreaded = singleThreadResourceCreate]()
				{
					dx12::Context* context = renderManager.GetContext()->As<dx12::Context*>();

					dx12::CommandQueue* copyQueue = &context->GetCommandQueue(dx12::CommandListType::Copy);

					SEBeginGPUEvent(copyQueue->GetD3DCommandQueue().Get(), 
						perfmarkers::Type::CopyQueue, 
						"Copy Queue: Create API Resources");

					std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();

					re::GPUTimer::Handle texCopyTimer = context->GetGPUTimer().StartCopyTimer(
						copyCommandList->GetD3DCommandList().Get(),
						"Copy textures",
						k_GPUFrameTimerName);

					if (!singleThreaded)
					{
						renderManager.m_newTextures.AquireReadLock();
					}
					for (auto& texture : renderManager.m_newTextures.GetReadData())
					{
						platform::Texture::CreateAPIResource(texture, copyCommandList.get());
					}
					if (!singleThreaded)
					{
						renderManager.m_newTextures.ReleaseReadLock();
					}

					texCopyTimer.StopTimer(copyCommandList->GetD3DCommandList().Get());

					copyQueue->Execute(1, &copyCommandList);

					SEEndGPUEvent(copyQueue->GetD3DCommandQueue().Get());
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
		size_t shaderTasksIdx = k_invalidCreateTaskIdx;
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
				shaderTasksIdx = createTasks.size();
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
						vertexStream->CreateBuffers(vertexStream);
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
		// Acceleration structures:
		if (renderManager.m_newAccelerationStructures.HasReadData())
		{
			auto CreateAccelerationStructures = [&renderManager, singleThreaded = singleThreadResourceCreate]()
				{
					if (!singleThreaded)
					{
						renderManager.m_newAccelerationStructures.AquireReadLock();
					}
					for (auto& accelStructure : renderManager.m_newAccelerationStructures.GetReadData())
					{
						platform::AccelerationStructure::Create(*accelStructure);
					}
					if (!singleThreaded)
					{
						renderManager.m_newAccelerationStructures.ReleaseReadLock();
					}
				};

			if (singleThreadResourceCreate)
			{
				CreateAccelerationStructures();
			}
			else
			{
				createTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(CreateAccelerationStructures));
			}
		}
		// Shader binding tables:
		if (renderManager.m_newShaderBindingTables.HasReadData())
		{
			auto CreateShaderBindingTables =
				[&renderManager, &createTasks, shaderTasksIdx, singleThreaded = singleThreadResourceCreate]()
				{
					// Shader binding tables require shaders to have already been loaded (as they access their loaded
					// blobs etc). We must wait for loading to be complete before proceeding
					if (!singleThreaded &&
						shaderTasksIdx != k_invalidCreateTaskIdx)
					{
						createTasks[shaderTasksIdx].wait();
					}

					if (!singleThreaded)
					{
						renderManager.m_newShaderBindingTables.AquireReadLock();
					}
					for (auto& sbt : renderManager.m_newShaderBindingTables.GetReadData())
					{
						dx12::ShaderBindingTable::Create(*sbt);
					}
					if (!singleThreaded)
					{
						renderManager.m_newShaderBindingTables.ReleaseReadLock();
					}
				};

			if (singleThreadResourceCreate)
			{
				CreateShaderBindingTables();
			}
			else
			{
				createTasks.emplace_back(core::ThreadPool::Get()->EnqueueJob(CreateShaderBindingTables));
			}
		}

		SEAssert(createTasks.size() <= k_createTasksReserveAmt,
			"Too many create tasks, vector may have been reallocated: k_createTasksReserveAmt must be updated");

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

		dx12::Context* context = renderManager.GetContext()->As<dx12::Context*>();

		context->GetHeapManager().EndOfFrame(renderManager.GetCurrentRenderFrameNum());

		SEEndCPUEvent();
	}


	void RenderManager::Render()
	{
		dx12::Context* context = GetContext()->As<dx12::Context*>();

		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);
		dx12::CommandQueue& computeQueue = context->GetCommandQueue(dx12::CommandListType::Compute);

		std::vector<std::future<std::shared_ptr<dx12::CommandList>>> commandListJobs;

		re::GPUTimer& gpuTimer = context->GetGPUTimer();
		re::GPUTimer::Handle frameTimer;


		auto StageTypeToCommandListType = [](const re::Stage::Type stageType) -> dx12::CommandListType
			{
				SEStaticAssert(static_cast<uint8_t>(re::Stage::Type::Invalid) == 10,
					"Number of stage types has changed. This must be updated");

				switch (stageType)
				{
				case re::Stage::Type::Graphics:
				case re::Stage::Type::LibraryGraphics:
				case re::Stage::Type::FullscreenQuad:
				case re::Stage::Type::ClearTargetSet: // All clears are currently done on the graphics queue
				case re::Stage::Type::Copy: // All copies are currently done on the graphics queue
					return dx12::CommandListType::Direct;
				case re::Stage::Type::Compute:
				case re::Stage::Type::LibraryCompute:
				case re::Stage::Type::ClearRWTextures:
				case re::Stage::Type::RayTracing:
					return dx12::CommandListType::Compute;
				case re::Stage::Type::Parent:
				case re::Stage::Type::Invalid:
				default: SEAssertF("Unexpected stage type");
				}
				return dx12::CommandListType_Invalid; // This should never happen
			};

		auto IsGraphicsQueueStageType = [&StageTypeToCommandListType](const re::Stage::Type stageType) -> bool
			{
				return StageTypeToCommandListType(stageType) == dx12::CommandListType::Direct;
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

		// A WorkRange spans a contiguous subset of the Stages within a single StagePipeline
		struct WorkRange
		{
			re::RenderPipeline const* m_renderPipeline;
			std::vector<re::StagePipeline>::const_iterator m_stagePipelineItr;
			std::list<std::shared_ptr<re::Stage>>::const_iterator m_stageBeginItr;
			std::list<std::shared_ptr<re::Stage>>::const_iterator m_stageEndItr;
		};


		auto RecordCommandList = [this, context, &StageTypeToCommandListType](
			std::vector<WorkRange>&& workRangeIn,
			std::shared_ptr<dx12::CommandList>&& cmdListIn)
				-> std::shared_ptr<dx12::CommandList>
			{
				std::shared_ptr<dx12::CommandList> cmdList = std::move(cmdListIn);

				// We move the WorkRange here to ensure it is cleared even if we're recording single-threaded
				const std::vector<WorkRange> workRange = std::move(workRangeIn);

				SEAssert(!workRange.empty(), "Work range is empty");

				auto SetDrawState = [&context](
					re::Stage const* stage,
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
						commandList->SetBuffers(stage->GetPermanentBuffers());
						commandList->SetBuffers(stage->GetPerFrameBuffers());

						// Set inputs and targets (once) now that the root signature is set
						if (doSetStageInputsAndTargets)
						{
							const int depthTargetTexInputIdx = stage->GetDepthTargetTextureInputIdx();

							commandList->SetTextures(stage->GetPermanentTextureInputs(), depthTargetTexInputIdx);
							commandList->SetTextures(stage->GetSingleFrameTextureInputs(), depthTargetTexInputIdx);

							commandList->SetRWTextures(stage->GetPermanentRWTextureInputs());
							commandList->SetRWTextures(stage->GetSingleFrameRWTextureInputs());

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
							case re::Stage::Type::ClearTargetSet:
							{
								commandList->SetRenderTargets(*targetSet);
							}
							break;
							default:
								SEAssertF("Invalid stage type");
							}
						}

						// Set root constants:
						commandList->SetRootConstants(stage->GetRootConstants());
					};			


				// All stages in a range are recorded to the same queue/command list type
				
				const dx12::CommandListType cmdListType = cmdList->GetCommandListType();
				SEAssert(cmdListType == StageTypeToCommandListType((*workRange[0].m_stageBeginItr)->GetStageType()),
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

						renderPipelineTimer.StopTimer(cmdList->GetD3DCommandList().Get());

						renderPipelineTimer = gpuTimer.StartTimer(cmdList->GetD3DCommandList().Get(),
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

						stagePipelineTimer.StopTimer(cmdList->GetD3DCommandList().Get());

						stagePipelineTimer = gpuTimer.StartTimer(cmdList->GetD3DCommandList().Get(),
							stagePipeline.GetName().c_str(),
							renderPipeline->GetName().c_str());

						SEBeginGPUEvent( // StagePipeline
							cmdList->GetD3DCommandList().Get(),
							perfMarkerType,
							stagePipeline.GetName().c_str());
					}
					const bool isLastOfStagePipeline = isLastWorkEntry ||
						lastSeenStagePipeline != &(*std::next(workRangeItr)->m_stagePipelineItr);
					
					// Stage ranges are contiguous within a single StagePipeline
					auto stageItr = workRangeItr->m_stageBeginItr;
					while (stageItr != workRangeItr->m_stageEndItr)
					{
						SEBeginGPUEvent( // Stage
							cmdList->GetD3DCommandList().Get(),
							perfMarkerType,
							(*stageItr)->GetName().c_str());

						re::GPUTimer::Handle stageTimer = gpuTimer.StartTimer(cmdList->GetD3DCommandList().Get(),
							(*stageItr)->GetName().c_str(),
							stagePipeline.GetName().c_str());

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
						cmdList->RecordStageName(stage->GetName());
#endif

						const re::Stage::Type curStageType = (*stageItr)->GetStageType();
						switch (curStageType)
						{
						case re::Stage::Type::LibraryGraphics: // Library stages are executed with their own internal logic
						case re::Stage::Type::LibraryCompute:
						{
							cmdList->SetRootConstants((*stageItr)->GetRootConstants());

							dynamic_cast<re::LibraryStage*>((*stageItr).get())->Execute(cmdList.get());
						}
						break;
						case re::Stage::Type::ClearTargetSet:
						{
							SEAssert(cmdList->GetCommandListType() == dx12::CommandListType::Direct,
								"Incorrect command list type");

							// Note: We do not have to have SetRenderTargets() to clear them in DX12

							re::ClearTargetSetStage const* clearStage = 
								dynamic_cast<re::ClearTargetSetStage const*>((*stageItr).get());
							SEAssert(clearStage, "Failed to get clear stage");

							cmdList->ClearTargets(
								clearStage->GetAllColorClearModes(),
								clearStage->GetAllColorClearValues(),
								clearStage->GetNumColorClearElements(),
								clearStage->DepthClearEnabled(),
								clearStage->GetDepthClearValue(),
								clearStage->StencilClearEnabled(),
								clearStage->GetStencilClearValue(),
								*(*stageItr)->GetTextureTargetSet());
						}
						break;
						case re::Stage::Type::ClearRWTextures:
						{
							SEAssert(cmdList->GetCommandListType() == dx12::CommandListType::Compute,
								"Incorrect command list type");

							re::ClearRWTexturesStage const* clearStage =
								dynamic_cast<re::ClearRWTexturesStage const*>((*stageItr).get());
							SEAssert(clearStage, "Failed to get clear stage");

							void const* clearValue = clearStage->GetClearValue();
							switch (clearStage->GetClearValueType())
							{
							case re::ClearRWTexturesStage::ValueType::Float:
							{
								cmdList->ClearUAV(clearStage->GetPermanentRWTextureInputs(),
									*static_cast<glm::vec4 const*>(clearValue));
								cmdList->ClearUAV(clearStage->GetSingleFrameRWTextureInputs(),
									*static_cast<glm::vec4 const*>(clearValue));
							}
							break;
							case re::ClearRWTexturesStage::ValueType::Uint:
							{
								cmdList->ClearUAV(clearStage->GetPermanentRWTextureInputs(),
									*static_cast<glm::uvec4 const*>(clearValue));
								cmdList->ClearUAV(clearStage->GetSingleFrameRWTextureInputs(),
									*static_cast<glm::uvec4 const*>(clearValue));
							}
							break;
							default: SEAssertF("Invalid clear value type");
							}
						}
						break;
						case re::Stage::Type::Copy:
						{
							re::CopyStage const* copyStage = dynamic_cast<re::CopyStage const*>((*stageItr).get());
							SEAssert(copyStage, "Failed to get clear stage");

							core::InvPtr<re::Texture> dstTexture = copyStage->GetDstTexture();
							if (!dstTexture.IsValid()) // If no valid destination is provided, we use the backbuffer
							{
								re::TextureTargetSet const* backbufferTargets = 
									dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain()).get();

								dstTexture = backbufferTargets->GetColorTarget(0).GetTexture();
							}

							cmdList->CopyTexture(copyStage->GetSrcTexture(), dstTexture);
						}
						break;
						case re::Stage::Type::RayTracing:
						{
							re::Stage::RayTracingStageParams const* rtStageParams = 
								dynamic_cast<re::Stage::RayTracingStageParams const*>((*stageItr)->GetStageParams());
							SEAssert(rtStageParams, "Failed to cast to RayTracingStageParams parameters");

							std::vector<re::Batch> const& batches = (*stageItr)->GetStageBatches();
							for (size_t batchIdx = 0; batchIdx < batches.size(); batchIdx++)
							{
								re::Batch const& batch = batches[batchIdx];

								re::Batch::RayTracingParams const& batchRTParams = batch.GetRayTracingParams();
								
								SEAssert(batchRTParams.m_ASInput.m_accelerationStructure,
									"AccelerationStructure is null");

								switch (batchRTParams.m_operation)
								{
								case re::Batch::RayTracingParams::Operation::BuildAS:
								{
									cmdList->BuildRaytracingAccelerationStructure(
										*batchRTParams.m_ASInput.m_accelerationStructure, false);
								}
								break;
								case re::Batch::RayTracingParams::Operation::UpdateAS:
								{
									cmdList->BuildRaytracingAccelerationStructure(
										*batchRTParams.m_ASInput.m_accelerationStructure, true);
								}
								break;
								case re::Batch::RayTracingParams::Operation::CompactAS:
								{
									SEAssertF("TODO: Implement this");
								}
								break;
								case re::Batch::RayTracingParams::Operation::DispatchRays:
								{
									SEAssert(!batchRTParams.m_ASInput.m_shaderName.empty(),
										"Acceleration structure input shader name is empty");

									SEAssert(batchRTParams.m_ASInput.m_accelerationStructure->GetShaderBindingTable(),
										"ShaderBindingTable is null");

									SEAssert(batchRTParams.m_dispatchDimensions.x > 0 || 
										batchRTParams.m_dispatchDimensions.y > 0 ||
										batchRTParams.m_dispatchDimensions.z > 0,
										"Dispatch dimensions cannot be 0");
									
									cmdList->AttachBindlessResources(
										*batchRTParams.m_ASInput.m_accelerationStructure->GetShaderBindingTable(),
										*context->GetBindlessResourceManager());

									cmdList->SetRootConstants((*stageItr)->GetRootConstants());
									cmdList->SetRootConstants(batch.GetRootConstants());

									cmdList->DispatchRays(
										*batchRTParams.m_ASInput.m_accelerationStructure->GetShaderBindingTable(),
										batchRTParams.m_dispatchDimensions,
										batchRTParams.m_rayGenShaderIdx);
								}
								break;
								default: SEAssertF("Invalid ray tracing batch operation type");
								}
							}
						}
						break;
						case re::Stage::Type::Graphics:
						case re::Stage::Type::FullscreenQuad:
						case re::Stage::Type::Compute:
						{
							// Get the stage targets:
							re::TextureTargetSet const* stageTargets = (*stageItr)->GetTextureTargetSet();
							if (stageTargets == nullptr && curStageType != re::Stage::Type::Compute)
							{
								stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain()).get();
							}
							SEAssert(stageTargets || curStageType == re::Stage::Type::Compute,
								"The current stage does not have targets set. This is unexpected");

							core::InvPtr<re::Shader> currentShader;
							bool hasSetStageInputsAndTargets = false;

							// Stage batches:
							std::vector<re::Batch> const& batches = (*stageItr)->GetStageBatches();
							for (size_t batchIdx = 0; batchIdx < batches.size(); batchIdx++)
							{
								core::InvPtr<re::Shader> const& batchShader = batches[batchIdx].GetShader();
								SEAssert(batchShader != nullptr, "Batch must have a shader");

								if (currentShader != batchShader)
								{
									currentShader = batchShader;

									SetDrawState(
										(*stageItr).get(),
										curStageType,
										currentShader,
										stageTargets,
										cmdList.get(),
										!hasSetStageInputsAndTargets);
									hasSetStageInputsAndTargets = true;
								}
								SEAssert(currentShader, "Current shader is null");

								// Batch buffers:
								cmdList->SetBuffers(batches[batchIdx].GetBuffers());

								// Batch Texture / Sampler inputs :
#if defined (_DEBUG)
								for (auto const& texSamplerInput : batches[batchIdx].GetTextureAndSamplerInputs())
								{
									SEAssert(!stageTargets->HasDepthTarget() ||
										texSamplerInput.m_texture != stageTargets->GetDepthStencilTarget().GetTexture(),
										"We don't currently handle batches with the current depth buffer attached as "
										"a texture input. We need to make sure skipping transitions is handled correctly here");
								}
#endif
								cmdList->SetTextures(batches[batchIdx].GetTextureAndSamplerInputs());

								// Batch compute inputs:
								cmdList->SetRWTextures(batches[batchIdx].GetRWTextureInputs());

								// Set root constants:
								cmdList->SetRootConstants(batches[batchIdx].GetRootConstants());

								switch (curStageType)
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
						break;
						default: SEAssertF("Unexpected stage type");
						}

						stageTimer.StopTimer(cmdList->GetD3DCommandList().Get());
						SEEndGPUEvent(cmdList->GetD3DCommandList().Get()); // Stage

						++stageItr;
					}

					if (isLastOfStagePipeline)
					{
						stagePipelineTimer.StopTimer(cmdList->GetD3DCommandList().Get());
						SEEndGPUEvent(cmdList->GetD3DCommandList().Get()); // StagePipeline
					}

					if (isLastOfRenderSystem)
					{
						renderPipelineTimer.StopTimer(cmdList->GetD3DCommandList().Get());
						// No RenderSystem GPUEvent marker to end
					}
					
					++workRangeItr;
				}

				return cmdList;
			};

		auto EnqueueWorkRecording = 
			[&commandListJobs, &RecordCommandList, &StageTypeToCommandListType, &directQueue, &computeQueue, &frameTimer]
			(std::vector<WorkRange>&& workRange, bool startGPUFrameTimer, bool stopGPUFrameTimer)
			{
				if (workRange.empty())
				{
					return;
				}

				const dx12::CommandListType cmdListType =
					StageTypeToCommandListType((*workRange[0].m_stageBeginItr)->GetStageType());

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
					// This lambda is called from Render(), a member function of dx12::RenderManager.
					// 'this->GetContext()' or just 'GetContext()' should resolve correctly.
					// frameTimer is captured by reference.
					frameTimer = GetContext()->GetGPUTimer().StartTimer(
						cmdList->GetD3DCommandList().Get(),
						k_GPUFrameTimerName);
				}

				static const bool s_recordSingleThreaded = 
					core::Config::Get()->KeyExists(core::configkeys::k_singleThreadCmdListRecording);
				if (s_recordSingleThreaded)
				{
					cmdList = RecordCommandList(std::move(workRange), std::move(cmdList));

					if (stopGPUFrameTimer)
					{
						frameTimer.StopTimer(cmdList->GetD3DCommandList().Get());
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
								frameTimer.StopTimer(populatedCmdList->GetD3DCommandList().Get());
							}

							return populatedCmdList;
						}));
				}
			};


		// Populate sets of WorkRanges that can be recorded on the same command list. A single WorkRange spans a
		// contiguous subset of the Stages of a single StagePipeline, we asyncronously record all work on a single 
		// command list and then immediately execute it when we detect the command list type has changed
		std::vector<WorkRange> workRange;

		re::Stage::Type prevStageType = re::Stage::Type::Invalid;
		bool mustStartFrameTimer = true;

		auto renderSystemItr = m_renderSystems.begin();
		while (renderSystemItr != m_renderSystems.end())
		{
			re::RenderPipeline& renderPipeline = (*renderSystemItr)->GetRenderPipeline();

			auto stagePipelineItr = renderPipeline.GetStagePipeline().begin();
			while (stagePipelineItr != renderPipeline.GetStagePipeline().end())
			{
				std::list<std::shared_ptr<re::Stage>> const& stages = (*stagePipelineItr).GetStages();
				if (stages.empty())
				{
					++stagePipelineItr;
					continue;
				}

				auto stageStartItr = stages.begin();
				auto stageEndItr = stages.begin();
				while (stageEndItr != stages.end())
				{
					// Skip empty stages:
					if ((*stageEndItr)->IsSkippable())
					{
						// If we've traversed beyond the 1st element, record some work:
						if (stageEndItr != stageStartItr)
						{
							workRange.emplace_back(WorkRange{
								.m_renderPipeline = &(*renderSystemItr)->GetRenderPipeline(),
								.m_stagePipelineItr = stagePipelineItr,
								.m_stageBeginItr = stageStartItr,
								.m_stageEndItr = stageEndItr, });
						}

						++stageEndItr; // This element is skipable: Advance before we update stageStartItr
						stageStartItr = stageEndItr;

						continue;
					}

					// We've found our first valid Stage: Initialize our state:
					if (prevStageType == re::Stage::Type::Invalid)
					{
						prevStageType = (*stageEndItr)->GetStageType();
						SEAssert(prevStageType != re::Stage::Type::Invalid, "Invalid stage type");
					}

					const re::Stage::Type curStageType = (*stageEndItr)->GetStageType();
					const bool cmdListTypeChanged = CmdListTypeChanged(prevStageType, curStageType);
					if (cmdListTypeChanged)
					{
						// If we've traversed beyond the 1st element, record some work:
						if (stageEndItr != stageStartItr)
						{
							workRange.emplace_back(WorkRange{
								.m_renderPipeline = &(*renderSystemItr)->GetRenderPipeline(),
								.m_stagePipelineItr = stagePipelineItr,
								.m_stageBeginItr = stageStartItr,
								.m_stageEndItr = stageEndItr, });
						}

						EnqueueWorkRecording(std::move(workRange), mustStartFrameTimer, false);
						mustStartFrameTimer = false;

						stageStartItr = stageEndItr;
						prevStageType = curStageType;
					}

					++stageEndItr;

					const bool isLastStage = stageEndItr == stages.end();
					if (isLastStage)
					{
						workRange.emplace_back(WorkRange{
							.m_renderPipeline = &(*renderSystemItr)->GetRenderPipeline(),
							.m_stagePipelineItr = stagePipelineItr,
							.m_stageBeginItr = stageStartItr,
							.m_stageEndItr = stages.end(), });
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

		GetContext()->GetGPUTimer().EndFrame();
	}


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
		// Note: Shutdown order matters. Make sure any work performed here plays nicely with the 
		// re::RenderManager::Shutdown ordering

		dx12::Context* context = renderManager.GetContext()->As<dx12::Context*>();
		
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
