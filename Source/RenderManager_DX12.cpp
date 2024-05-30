// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "Buffer_DX12.h"
#include "ProfilingMarkers.h"
#include "RenderManager_DX12.h"
#include "RenderSystem.h"
#include "Sampler_DX12.h"
#include "Shader_DX12.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"
#include "VertexStream_DX12.h"

#include "Core\Assert.h"

#include <d3dx12.h>

using Microsoft::WRL::ComPtr;


namespace dx12
{
	RenderManager::RenderManager()
		: re::RenderManager(platform::RenderingAPI::DX12)
		, k_numFrames(core::Config::Get()->GetValue<int>(core::configkeys::k_numBackbuffersKey))
	{
		SEAssert(k_numFrames >= 2 && k_numFrames <= 3, "Invalid number of frames in flight");

		m_intermediateResources.resize(k_numFrames);
		m_intermediateResourceFenceVals.resize(k_numFrames, 0);
	}


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// Prepend DX12-specific render systems:
		const std::string dx12PlatformPipelineFileName = core::configkeys::k_platformPipelineFileName_DX12;
		constexpr char const* k_dx12RenderSystemName = "PlatformDX12";
		renderManager.CreateAddRenderSystem(k_dx12RenderSystemName, dx12PlatformPipelineFileName);
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Note: We've already obtained the read lock on all new resources by this point

		dx12::RenderManager& dx12RenderManager = dynamic_cast<dx12::RenderManager&>(renderManager);
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		dx12::CommandQueue* copyQueue = &context->GetCommandQueue(dx12::CommandListType::Copy);

		SEBeginGPUEvent(copyQueue->GetD3DCommandQueue(), perfmarkers::Type::CopyQueue, "Copy Queue: Create API Resources");

		// Ensure any updates using the intermediate resources created during the previous frame are done
		const uint8_t intermediateIdx = GetIntermediateResourceIdx();
		if (!copyQueue->GetFence().IsFenceComplete(
			dx12RenderManager.m_intermediateResourceFenceVals[intermediateIdx]))
		{
			copyQueue->CPUWait(dx12RenderManager.m_intermediateResourceFenceVals[intermediateIdx]);
		}
		dx12RenderManager.m_intermediateResources[intermediateIdx].clear();

		const bool hasDataToCopy = 
			renderManager.m_newVertexStreams.HasReadData() ||
			renderManager.m_newTextures.HasReadData();

		// Handle anything that requires a copy queue:		
		if (hasDataToCopy)
		{
			std::vector<ComPtr<ID3D12Resource>>& intermediateResources = 
				dx12RenderManager.m_intermediateResources[intermediateIdx];

			// TODO: Get multiple command lists, and record on multiple threads:
			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();

			// Vertex streams:
			if (renderManager.m_newVertexStreams.HasReadData())
			{
				for (auto& newVertexStream : renderManager.m_newVertexStreams.GetReadData())
				{
					dx12::VertexStream::Create(*newVertexStream, copyCommandList.get(), intermediateResources);
				}
			}

			// Textures:
			if (renderManager.m_newTextures.HasReadData())
			{
				for (auto& texture : renderManager.m_newTextures.GetReadData())
				{
					dx12::Texture::Create(*texture, copyCommandList.get(), intermediateResources);
				}
			}

			// Execute the copy before moving on
			dx12RenderManager.m_intermediateResourceFenceVals[intermediateIdx] = copyQueue->Execute(1, &copyCommandList);
		}

		// Samplers:
		if (renderManager.m_newSamplers.HasReadData())
		{
			for (auto& newObject : renderManager.m_newSamplers.GetReadData())
			{
				dx12::Sampler::Create(*newObject);
			}
		}
		// Texture Target Sets:
		if (renderManager.m_newTargetSets.HasReadData())
		{
			for (auto& newObject : renderManager.m_newTargetSets.GetReadData())
			{
				newObject->Commit();
				dx12::TextureTargetSet::CreateColorTargets(*newObject);
				dx12::TextureTargetSet::CreateDepthStencilTarget(*newObject);
			}
		}
		// Shaders:
		if (renderManager.m_newShaders.HasReadData())
		{
			for (auto& shader : renderManager.m_newShaders.GetReadData())
			{
				dx12::Shader::Create(*shader);
			}
		}
		// Buffers:
		if (renderManager.m_newBuffers.HasReadData())
		{
			for (auto& newObject : renderManager.m_newBuffers.GetReadData())
			{
				dx12::Buffer::Create(*newObject);
			}
		}

		SEEndGPUEvent(copyQueue->GetD3DCommandQueue());
	}


	void RenderManager::Render()
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);
		dx12::CommandQueue& computeQueue = context->GetCommandQueue(dx12::CommandListType::Compute);

		std::vector<std::shared_ptr<dx12::CommandList>> commandLists;
		std::shared_ptr<dx12::CommandList> directCommandList = nullptr;
		std::shared_ptr<dx12::CommandList> computeCommandList = nullptr;
		
		auto AppendCommandLists = [&]()
		{
			if (computeCommandList != nullptr)
			{
				commandLists.emplace_back(computeCommandList);
				SEEndGPUEvent(computeCommandList->GetD3DCommandList()); // StagePipeline

			}
			if (directCommandList != nullptr)
			{
				commandLists.emplace_back(directCommandList);
				SEEndGPUEvent(directCommandList->GetD3DCommandList()); // StagePipeline
			}
		};

		auto IsGraphicsQueueStageType = [](const re::RenderStage::Type stageType) -> bool
			{
				switch (stageType)
				{
				case re::RenderStage::Type::Graphics:
				case re::RenderStage::Type::FullscreenQuad:
				case re::RenderStage::Type::Clear:
					return true;
				case re::RenderStage::Type::Parent:
				case re::RenderStage::Type::Compute:
					return false;
				default: SEAssertF("Invalid stage type");
				}
				return false;
			};

		auto StageTypeChanged = [IsGraphicsQueueStageType](
			const re::RenderStage::Type prev, const re::RenderStage::Type current) -> bool
			{
				return prev != re::RenderStage::Type::Invalid && // First iteration
					(prev != current &&
					!(IsGraphicsQueueStageType(prev) && IsGraphicsQueueStageType(current)));
			};

		re::RenderStage::Type prevRenderStageType = re::RenderStage::Type::Invalid;

		// Render each RenderSystem in turn:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			// Render each stage in the RenderSystem's RenderPipeline:
			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();
			for (re::StagePipeline& stagePipeline : renderPipeline.GetStagePipeline())
			{
				// Note: Our command lists and associated command allocators are already closed/reset
				directCommandList = nullptr;
				computeCommandList = nullptr;

				// Process all of the RenderStages attached to the StagePipeline:
				std::list<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
				for (std::shared_ptr<re::RenderStage> const& renderStage : stagePipeline.GetRenderStages())
				{
					const re::RenderStage::Type curRenderStageType = renderStage->GetStageType();

					// Library stages are executed with their own internal logic:
					if (curRenderStageType == re::RenderStage::Type::Library)
					{
						// TODO: There is an ordering issue here: LibraryStages (currently) create and submit their own
						// command lists. If they're part of a RenderSystem with stages before/after, they'll be
						// rendered in the wrong order. Currently, our Library stages are contained in their own
						// RenderSystems, but they don't need to be
						dynamic_cast<re::LibraryStage*>(renderStage.get())->Execute();
						continue;
					}

					// Skip empty stages:
					if (renderStage->IsSkippable())
					{
						continue;
					}

					// If the new RenderStage type is different to the previous one, we need to end recording on it
					// to ensure the work is correctly ordered between queues:
					if (StageTypeChanged(prevRenderStageType, curRenderStageType))
					{
						AppendCommandLists();

						computeCommandList = nullptr;
						directCommandList = nullptr;
					}
					prevRenderStageType = curRenderStageType;

					// Get a CommandList for the current RenderStage:
					dx12::CommandList* currentCommandList = nullptr;
					switch (curRenderStageType)
					{
					case re::RenderStage::Type::Graphics:
					case re::RenderStage::Type::FullscreenQuad:
					case re::RenderStage::Type::Clear:
					{
						if (directCommandList == nullptr)
						{
							directCommandList = directQueue.GetCreateCommandList();

							SEBeginGPUEvent( // Add a marker to wrap the StagePipeline
								directCommandList->GetD3DCommandList(),
								perfmarkers::Type::GraphicsCommandList,
								stagePipeline.GetName().c_str());
						}
						currentCommandList = directCommandList.get();
						SEBeginGPUEvent( // Add a marker to wrap the RenderStage
							currentCommandList->GetD3DCommandList(),
							perfmarkers::Type::GraphicsCommandList,
							renderStage->GetName().c_str());
					}
					break;
					case re::RenderStage::Type::Compute:
					{
						if (computeCommandList == nullptr)
						{
							computeCommandList = computeQueue.GetCreateCommandList();

							SEBeginGPUEvent( // Add a marker to wrap the StagePipeline
								computeCommandList->GetD3DCommandList(),
								perfmarkers::Type::GraphicsCommandList,
								stagePipeline.GetName().c_str());
						}
						currentCommandList = computeCommandList.get();
						SEBeginGPUEvent( // Add a marker to wrap the RenderStage
							currentCommandList->GetD3DCommandList(),
							perfmarkers::Type::ComputeCommandList,
							renderStage->GetName().c_str());
					}
					break;
					default:
						SEAssertF("Invalid stage type");
					}

					// Get the stage targets:
					re::TextureTargetSet const* stageTargets = renderStage->GetTextureTargetSet();
					if (stageTargets == nullptr && curRenderStageType != re::RenderStage::Type::Compute)
					{
						stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain()).get();
					}
					SEAssert(stageTargets || curRenderStageType == re::RenderStage::Type::Compute,
						"The current stage does not have targets set. This is unexpected");


					auto SetDrawState = [&renderStage, &context, &curRenderStageType](
						re::Shader const* shader,
						re::TextureTargetSet const* targetSet,
						dx12::CommandList* commandList,
						bool doSetStageInputsAndTargets)
					{
						// Set the pipeline state and root signature first:
						dx12::PipelineState const* pso = context->GetPipelineStateObject(*shader, targetSet);
						commandList->SetPipelineState(*pso);

						switch (curRenderStageType)
						{
						case re::RenderStage::Type::Graphics:
						case re::RenderStage::Type::FullscreenQuad:
						{
							commandList->SetGraphicsRootSignature(dx12::Shader::GetRootSignature(*shader));
						}
						break;
						case re::RenderStage::Type::Compute:
						{
							commandList->SetComputeRootSignature(dx12::Shader::GetRootSignature(*shader));
						}
						break;
						default:
							SEAssertF("Invalid render stage type");
						}

						// Set buffers (Must happen after the root signature is set):
						for (std::shared_ptr<re::Buffer const> const& buffer : renderStage->GetPermanentBuffers())
						{
							commandList->SetBuffer(buffer.get());
						}
						for (std::shared_ptr<re::Buffer const> const& buffer : renderStage->GetPerFrameBuffers())
						{
							commandList->SetBuffer(buffer.get());
						}

						// Set inputs and targets (once) now that the root signature is set
						if (doSetStageInputsAndTargets)
						{
							// Set stage texture inputs:
							std::vector<re::RenderStage::RenderStageTextureAndSamplerInput> const& texInputs =
								renderStage->GetTextureInputs();
							const int depthTargetTexInputIdx = renderStage->GetDepthTargetTextureInputIdx();
							for (size_t texIdx = 0; texIdx < texInputs.size(); texIdx++)
							{
								// If the depth target is read-only, and we've also used it as an input to the stage, we
								// skip the resource transition (it's handled when binding the depth target as read only)
								const bool skipTransition = (texIdx == depthTargetTexInputIdx);

								commandList->SetTexture(
									texInputs[texIdx].m_shaderName,
									texInputs[texIdx].m_texture,
									texInputs[texIdx].m_srcMip,
									skipTransition);
								// Note: Static samplers have already been set during root signature creation
							}

							// Set the targets:
							switch (curRenderStageType)
							{
							case re::RenderStage::Type::Compute:
							{
								if (targetSet)
								{
									commandList->SetComputeTargets(*targetSet);
								}
							}
							break;
							case re::RenderStage::Type::Graphics:
							case re::RenderStage::Type::FullscreenQuad:
							case re::RenderStage::Type::Clear:
							{
								const bool attachDepthAsReadOnly = renderStage->DepthTargetIsAlsoTextureInput();

								commandList->SetRenderTargets(*targetSet, attachDepthAsReadOnly);
							}
							break;
							default:
								SEAssertF("Invalid stage type");
							}
						}
					};


					// Clear the targets
					switch (curRenderStageType)
					{
					case re::RenderStage::Type::Compute:
					{
						// TODO: Support compute target clearing
					}
					break;
					case re::RenderStage::Type::Graphics:
					case re::RenderStage::Type::FullscreenQuad:
					case re::RenderStage::Type::Clear:
					{
						// Note: We do not have to have SetRenderTargets() to clear them in DX12
						currentCommandList->ClearTargets(*stageTargets);
					}
					break;
					default:
						SEAssertF("Invalid stage type");
					}

					re::Shader const* currentShader = nullptr;
					bool hasSetStageInputsAndTargets = false;

					// RenderStage batches:
					std::vector<re::Batch> const& batches = renderStage->GetStageBatches();
					for (size_t batchIdx = 0; batchIdx < batches.size(); batchIdx++)
					{
						re::Shader const* batchShader = batches[batchIdx].GetShader();
						SEAssert(batchShader != nullptr, "Batch must have a shader");

						if (currentShader != batchShader)
						{
							currentShader = batchShader;

							SetDrawState(currentShader, stageTargets, currentCommandList, !hasSetStageInputsAndTargets);
							hasSetStageInputsAndTargets = true;
						}
						SEAssert(currentShader, "Current shader is null");

						// Batch buffers:
						std::vector<std::shared_ptr<re::Buffer>> const& batchBuffers =
							batches[batchIdx].GetBuffers();
						for (std::shared_ptr<re::Buffer> batchBuffer : batchBuffers)
						{
							currentCommandList->SetBuffer(batchBuffer.get());
						}

						// Batch Texture / Sampler inputs :
						for (auto const& texSamplerInput : batches[batchIdx].GetTextureAndSamplerInputs())
						{
							SEAssert(!stageTargets->HasDepthTarget() ||
								texSamplerInput.m_texture != stageTargets->GetDepthStencilTarget()->GetTexture().get(),
								"We don't currently handle batches with the current depth buffer attached as "
								"a texture input. We need to make sure the transitions are handled correctly");

							currentCommandList->SetTexture(
								texSamplerInput.m_shaderName,
								texSamplerInput.m_texture,
								texSamplerInput.m_srcMip,
								false);
							// Note: Static samplers have already been set during root signature creation
						}

						switch (curRenderStageType)
						{
						case re::RenderStage::Type::Graphics:
						case re::RenderStage::Type::FullscreenQuad:
						{
							currentCommandList->DrawBatchGeometry(batches[batchIdx]);
						}
						break;
						case re::RenderStage::Type::Compute:
						{
							currentCommandList->Dispatch(batches[batchIdx].GetComputeParams().m_threadGroupCount);
						}
						break;
						default:
							SEAssertF("Invalid render stage type");
						}
					}

					switch (curRenderStageType)
					{
					case re::RenderStage::Type::Graphics:
					case re::RenderStage::Type::FullscreenQuad:
					case re::RenderStage::Type::Clear:
					{
						SEEndGPUEvent(directCommandList->GetD3DCommandList()); // RenderStage
					}
					break;
					case re::RenderStage::Type::Compute:
					{
						SEEndGPUEvent(computeCommandList->GetD3DCommandList()); // RenderStage
					}
					break;
					default:
						SEAssertF("Invalid stage type");
					}
				}; // ProcessRenderStage

				// We're done: We've recorded a command list for the current StagePipeline
				AppendCommandLists();

			} // StagePipeline loop

			// Handle any GPU readbacks, now that all of the work has been recorded:


			// Submit command lists for the current render system, so work can begin while processing the next render
			// system. Command lists must be submitted on a single thread, and in the same order as the render stages 
			// they're generated from to ensure modification fences and GPU waits are are handled correctly
			SEBeginCPUEvent(std::format("Submit {} command lists ({})", 
				renderSystem->GetName(), 
				commandLists.size()).c_str());

			size_t startIdx = 0;
			while (startIdx < commandLists.size())
			{
				const CommandListType cmdListType = commandLists[startIdx]->GetCommandListType();

				// Find the index of the last command list of the same type:
				size_t endIdx = startIdx + 1;

				//#define SUBMIT_COMMANDLISTS_IN_SERIAL
#if !defined(SUBMIT_COMMANDLISTS_IN_SERIAL)
				while (endIdx < commandLists.size() &&
					commandLists[endIdx]->GetCommandListType() == cmdListType)
				{
					endIdx++;
				}
#endif

				SEBeginCPUEvent(std::format("Submit command lists {}-{}", startIdx, endIdx).c_str());

				const size_t numCmdLists = endIdx - startIdx;

				switch (cmdListType)
				{
				case CommandListType::Direct:
				{
					directQueue.Execute(static_cast<uint32_t>(numCmdLists), &commandLists[startIdx]);
				}
				break;
				case CommandListType::Bundle:
				{
					SEAssertF("TODO: Support this type");
				}
				break;
				case CommandListType::Compute:
				{
					computeQueue.Execute(static_cast<uint32_t>(numCmdLists), &commandLists[startIdx]);
				}
				break;
				case CommandListType::Copy:
				{
					SEAssertF("Currently not expecting to find a copy queue genereted from a render stage");
				}
				break;
				case CommandListType::CommandListType_Invalid:
				default:
					SEAssertF("Invalid command list type");
				}

				startIdx = endIdx;

				SEEndCPUEvent(); // Submit command list range
			}
			SEEndCPUEvent(); // Submit command lists

			// Clear the command lists recorded by the current render system
			// Note: These will all already be null, as their owning command queue has reclaimed them during submission
			commandLists.clear();

		} // m_renderSystems loop
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
