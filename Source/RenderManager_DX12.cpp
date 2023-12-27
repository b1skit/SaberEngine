// � 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "Context_DX12.h"
#include "Assert.h"
#include "Debug_DX12.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_ComputeMips.h"
#include "GraphicsSystem_Debug.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Tonemapping.h"
#include "ParameterBlock_DX12.h"
#include "ProfilingMarkers.h"
#include "RenderManager_DX12.h"
#include "RenderSystem.h"
#include "Sampler_DX12.h"
#include "Shader_DX12.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"
#include "VertexStream_DX12.h"

using re::RenderStage;
using re::StagePipeline;
using Microsoft::WRL::ComPtr;
using glm::vec4;
using std::make_shared;
using std::shared_ptr;
using std::vector;


namespace dx12
{
	RenderManager::RenderManager()
		: m_intermediateResourceFenceVal(0)
		, k_numFrames(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_numBackbuffersKey))
	{
		SEAssert("Invalid number of frames in flight", k_numFrames >= 2 && k_numFrames <= 3);
	}


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// Create and add our RenderSystems:
		renderManager.m_renderSystems.emplace_back(re::RenderSystem::Create("Default DX12 RenderSystem"));
		re::RenderSystem* defaultRenderSystem = renderManager.m_renderSystems.back().get();

		// Build the default render system initialization pipeline:
		auto DefaultRenderSystemInititialzePipeline = [](re::RenderSystem* defaultRS)
		{
			gr::GraphicsSystemManager& gsm = defaultRS->GetGraphicsSystemManager();

			std::vector<std::shared_ptr<gr::GraphicsSystem>>& graphicsSystems = gsm.GetGraphicsSystems();

			// Create and add graphics systems:
			std::shared_ptr<gr::ComputeMipsGraphicsSystem> computeMipsGS = 
				std::make_shared<gr::ComputeMipsGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(computeMipsGS);

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

			std::shared_ptr<gr::TonemappingGraphicsSystem> tonemappingGS = 
				std::make_shared<gr::TonemappingGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(tonemappingGS);

			std::shared_ptr<gr::DebugGraphicsSystem> debugGS = std::make_shared<gr::DebugGraphicsSystem>(&gsm);
			graphicsSystems.emplace_back(debugGS);
		};
		defaultRenderSystem->SetInitializePipeline(DefaultRenderSystemInititialzePipeline);


		// Build the default render system create pipeline:
		auto DefaultRenderSystemCreatePipeline = [](re::RenderSystem* defaultRS)
		{
			gr::GraphicsSystemManager& gsm = defaultRS->GetGraphicsSystemManager();

			// Get our GraphicsSystems:
			gr::ComputeMipsGraphicsSystem* computeMipsGS = gsm.GetGraphicsSystem<gr::ComputeMipsGraphicsSystem>();
			gr::GBufferGraphicsSystem* gbufferGS = gsm.GetGraphicsSystem<gr::GBufferGraphicsSystem>();
			gr::ShadowsGraphicsSystem* shadowGS = gsm.GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
			gr::DeferredLightingGraphicsSystem* deferredLightingGS = gsm.GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();
			gr::SkyboxGraphicsSystem* skyboxGS = gsm.GetGraphicsSystem<gr::SkyboxGraphicsSystem>();
			gr::BloomGraphicsSystem* bloomGS = gsm.GetGraphicsSystem<gr::BloomGraphicsSystem>();
			gr::TonemappingGraphicsSystem* tonemappingGS = gsm.GetGraphicsSystem<gr::TonemappingGraphicsSystem>();
			gr::DebugGraphicsSystem* debugGS = gsm.GetGraphicsSystem<gr::DebugGraphicsSystem>();

			// Build the creation pipeline:
			gsm.Create();

			computeMipsGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(computeMipsGS->GetName()));
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
		defaultRenderSystem->SetCreatePipeline(DefaultRenderSystemCreatePipeline);


		// Build the default render system update pipeline:
		auto UpdatePipeline = [](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();

			// Get our GraphicsSystems:
			gr::ComputeMipsGraphicsSystem* computeMipsGS = gsm.GetGraphicsSystem<gr::ComputeMipsGraphicsSystem>();
			gr::GBufferGraphicsSystem* gbufferGS = gsm.GetGraphicsSystem<gr::GBufferGraphicsSystem>();
			gr::ShadowsGraphicsSystem* shadowGS = gsm.GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
			gr::DeferredLightingGraphicsSystem* deferredLightingGS = gsm.GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();
			gr::SkyboxGraphicsSystem* skyboxGS = gsm.GetGraphicsSystem<gr::SkyboxGraphicsSystem>();
			gr::BloomGraphicsSystem* bloomGS = gsm.GetGraphicsSystem<gr::BloomGraphicsSystem>();
			gr::TonemappingGraphicsSystem* tonemappingGS = gsm.GetGraphicsSystem<gr::TonemappingGraphicsSystem>();
			gr::DebugGraphicsSystem* debugGS = gsm.GetGraphicsSystem<gr::DebugGraphicsSystem>();

			// Execute per-frame updates:
			gsm.PreRender();

			computeMipsGS->PreRender();
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

		dx12::RenderManager& dx12RenderManager = dynamic_cast<dx12::RenderManager&>(renderManager);
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		dx12::CommandQueue* copyQueue = &context->GetCommandQueue(dx12::CommandListType::Copy);

		SEBeginGPUEvent(copyQueue->GetD3DCommandQueue(), perfmarkers::Type::CopyQueue, "Copy Queue: Create API Resources");

		// Ensure any intermediate resources created on the previous frame are done. In practice this is not necessary,
		// but we include this check as a precaution since we're about to clear the intermediate resources
		if (!copyQueue->GetFence().IsFenceComplete(dx12RenderManager.m_intermediateResourceFenceVal))
		{
			copyQueue->CPUWait(dx12RenderManager.m_intermediateResourceFenceVal);
		}
		dx12RenderManager.m_intermediateResources.clear();	

		const bool hasDataToCopy = 
			renderManager.m_newVertexStreams.HasReadData() ||
			renderManager.m_newTextures.HasReadData();

		// Handle anything that requires a copy queue:		
		std::vector<ComPtr<ID3D12Resource>>& intermediateResources = dx12RenderManager.m_intermediateResources;
		if (hasDataToCopy)
		{
			// TODO: Get multiple command lists, and record on multiple threads:
			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();
			ID3D12GraphicsCommandList2* copyCommandListD3D = copyCommandList->GetD3DCommandList();

			// Vertex streams:
			if (renderManager.m_newVertexStreams.HasReadData())
			{
				for (auto& newVertexStream : renderManager.m_newVertexStreams.GetReadData())
				{
					dx12::VertexStream::Create(*newVertexStream, copyCommandListD3D, intermediateResources);
				}
			}

			// Textures:
			if (renderManager.m_newTextures.HasReadData())
			{
				for (auto& texture : renderManager.m_newTextures.GetReadData())
				{
					dx12::Texture::Create(*texture, copyCommandListD3D, intermediateResources);
				}
			}

			// Execute the copy before moving on
			dx12RenderManager.m_intermediateResourceFenceVal = copyQueue->Execute(1, &copyCommandList);
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
				// Create the Shader object:
				dx12::Shader::Create(*shader);

				// Create any necessary PSO's for the Shader:
				for (std::unique_ptr<re::RenderSystem>& renderSystem : renderManager.m_renderSystems)
				{
					re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();
					for (StagePipeline& stagePipeline : renderPipeline.GetStagePipeline())
					{
						std::list<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
						for (std::shared_ptr<re::RenderStage> const renderStage : renderStages)
						{
							// We assume either a RenderStage has a shader, or all batches rendered on a RenderStage will
							// have their own shader. So, we must create a PSO per Shader for each RenderStage with a null
							// Shader (as any Shader might be used there), or if the Shader is used by the RenderStage
							if (renderStage->GetStageShader() == nullptr ||
								renderStage->GetStageShader()->GetNameID() == shader->GetNameID())
							{
								std::shared_ptr<re::TextureTargetSet const> stageTargets = renderStage->GetTextureTargetSet();
								if (!stageTargets)
								{
									stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain());
								}

								context->CreateAddPipelineState(*shader, *stageTargets);
							}
						}
					}
				}
			}
		}
		// Parameter Blocks:
		if (renderManager.m_newParameterBlocks.HasReadData())
		{
			for (auto& newObject : renderManager.m_newParameterBlocks.GetReadData())
			{
				dx12::ParameterBlock::Create(*newObject);
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

		RenderStage::RenderStageType prevRenderStageType = RenderStage::RenderStageType::Invalid;

		// Render each RenderSystem in turn:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			// Render each stage in the RenderSystem's RenderPipeline:
			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();
			for (StagePipeline& stagePipeline : renderPipeline.GetStagePipeline())
			{
				// Note: Our command lists and associated command allocators are already closed/reset
				directCommandList = nullptr;
				computeCommandList = nullptr;

				// Process all of the RenderStages attached to the StagePipeline:
				std::list<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
				for (std::shared_ptr<re::RenderStage> const& renderStage : stagePipeline.GetRenderStages())
				{
					const RenderStage::RenderStageType curRenderStageType = renderStage->GetStageType();

					// Skip empty stages:
					if (renderStage->IsSkippable())
					{
						continue;
					}

					// If the new RenderStage type is different to the previous one, we need to end recording on it
					// to ensure the work is correctly ordered between queues:
					if (curRenderStageType != prevRenderStageType)
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
					case re::RenderStage::RenderStageType::Clear:
					case re::RenderStage::RenderStageType::Graphics:
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
					case re::RenderStage::RenderStageType::Compute:
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
					std::shared_ptr<re::TextureTargetSet const> stageTargets = renderStage->GetTextureTargetSet();
					if (stageTargets == nullptr)
					{
						SEAssert("Only the graphics queue/command lists can render to the backbuffer",
							curRenderStageType == re::RenderStage::RenderStageType::Graphics);

						stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain());
					}


					auto SetDrawState = [&renderStage, &context](
						re::Shader const* shader,
						re::TextureTargetSet const* targetSet,
						dx12::CommandList* commandList)
					{
						// Set the pipeline state and root signature first:
						std::shared_ptr<dx12::PipelineState> pso = context->GetPipelineStateObject(*shader, targetSet);
						commandList->SetPipelineState(*pso);

						switch (renderStage->GetStageType())
						{
						case re::RenderStage::RenderStageType::Graphics:
						{
							commandList->SetGraphicsRootSignature(dx12::Shader::GetRootSignature(*shader));
						}
						break;
						case re::RenderStage::RenderStageType::Compute:
						{
							commandList->SetComputeRootSignature(dx12::Shader::GetRootSignature(*shader));
						}
						break;
						default:
							SEAssertF("Invalid render stage type");
						}

						// Set parameter blocks (Must happen after the root signature is set):
						for (std::shared_ptr<re::ParameterBlock> permanentPB : renderStage->GetPermanentParameterBlocks())
						{
							commandList->SetParameterBlock(permanentPB.get());
						}
						for (std::shared_ptr<re::ParameterBlock> perFramePB : renderStage->GetPerFrameParameterBlocks())
						{
							commandList->SetParameterBlock(perFramePB.get());
						}

						// Set per-frame stage textures/sampler inputs:
						std::vector<RenderStage::RenderStageTextureAndSamplerInput> const& texInputs =
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
					};

					re::Shader* stageShader = renderStage->GetStageShader();
					const bool hasStageShader = stageShader != nullptr;

					// If we have a stage shader, we can set the stage PBs once for all batches
					if (hasStageShader)
					{
						SetDrawState(stageShader, stageTargets.get(), currentCommandList);
					}

					// Set targets, now that the pipeline is set
					switch (curRenderStageType)
					{
					case re::RenderStage::RenderStageType::Compute:
					{
						currentCommandList->SetComputeTargets(*stageTargets);
					}
					break;
					case re::RenderStage::RenderStageType::Clear:
					case re::RenderStage::RenderStageType::Graphics:
					{
						const bool attachDepthAsReadOnly = renderStage->DepthTargetIsAlsoTextureInput();

						currentCommandList->SetRenderTargets(*stageTargets, attachDepthAsReadOnly);

						re::PipelineState const& rePipelineState = stageShader->GetPipelineState();
					}
					break;
					default:
						SEAssertF("Invalid stage type");
					}

					// Render stage batches:
					std::vector<re::Batch> const& batches = renderStage->GetStageBatches();
					for (size_t batchIdx = 0; batchIdx < batches.size(); batchIdx++)
					{
						// No stage shader: Must set stage PBs for each batch
						if (!hasStageShader)
						{
							re::Shader const* batchShader = batches[batchIdx].GetShader();
							SEAssert("Batch must have a shader if the stage does not have a shader",
								batchShader != nullptr);

							SetDrawState(batchShader, stageTargets.get(), currentCommandList);
						}

						// Batch parameter blocks:
						vector<shared_ptr<re::ParameterBlock>> const& batchPBs = batches[batchIdx].GetParameterBlocks();
						for (shared_ptr<re::ParameterBlock> batchPB : batchPBs)
						{
							currentCommandList->SetParameterBlock(batchPB.get());
						}

						// Batch Texture / Sampler inputs :
						if (stageTargets->WritesColor())
						{
							for (auto const& texSamplerInput : batches[batchIdx].GetTextureAndSamplerInputs())
							{
								SEAssert("We don't currently handle batches with the current depth buffer attached as "
									"a texture input. We need to make sure the transitions are handled correctly", 
									!stageTargets->HasDepthTarget() || 
									texSamplerInput.m_texture != stageTargets->GetDepthStencilTarget()->GetTexture().get());

								currentCommandList->SetTexture(
									texSamplerInput.m_shaderName,
									texSamplerInput.m_texture,
									texSamplerInput.m_srcMip,
									false);
								// Note: Static samplers have already been set during root signature creation
							}
						}

						switch (curRenderStageType)
						{
						case re::RenderStage::RenderStageType::Graphics:
						{
							currentCommandList->DrawBatchGeometry(batches[batchIdx]);
						}
						break;
						case re::RenderStage::RenderStageType::Compute:
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
					case re::RenderStage::RenderStageType::Clear:
					case re::RenderStage::RenderStageType::Graphics:
					{
						SEEndGPUEvent(directCommandList->GetD3DCommandList()); // RenderStage
					}
					break;
					case re::RenderStage::RenderStageType::Compute:
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
		}


		// Command lists must be submitted on a single thread, and in the same order as the render stages they're
		// generated from to ensure modification fences and GPU waits are are handled correctly
		SEBeginCPUEvent(std::format("Submit command lists ({})", commandLists.size()).c_str());
		size_t startIdx = 0;
		while (startIdx < commandLists.size())
		{
			const CommandListType cmdListType = commandLists[startIdx]->GetCommandListType();

			// Find the index of the last command list of the same type:
			size_t endIdx = startIdx + 1;
			while (endIdx < commandLists.size() &&
				commandLists[endIdx]->GetCommandListType() == cmdListType)
			{
				endIdx++;
			}

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
			case CommandListType::VideoDecode:
			case CommandListType::VideoProcess:
			case CommandListType::VideoEncode:
			{
				SEAssertF("TODO: Support this type");
			}
			break;
			case CommandListType::CommandListType_Invalid:
			default:
				SEAssertF("Invalid command list type");
			}

			startIdx = endIdx;

			SEEndCPUEvent();
		}
		SEEndCPUEvent();
	}


	void RenderManager::StartImGuiFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}


	void RenderManager::RenderImGui()
	{
		// ImGui internal rendering
		ImGui::Render(); // Note: Does not touch the GPU/graphics API

		// Get our SE rendering objects:
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);
		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		// Draw directly to the swapchain backbuffer
		re::SwapChain const& swapChain = context->GetSwapChain();
		const bool attachDepthAsReadOnly = true;
		commandList->SetRenderTargets(*dx12::SwapChain::GetBackBufferTargetSet(swapChain), attachDepthAsReadOnly);

		// Configure the descriptor heap:
		ID3D12GraphicsCommandList2* d3dCommandList = commandList->GetD3DCommandList();

		ID3D12DescriptorHeap* descriptorHeaps[1] = { context->GetImGuiGPUVisibleDescriptorHeap() };
		d3dCommandList->SetDescriptorHeaps(1, descriptorHeaps);

		// Record our ImGui draws:
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3dCommandList);

		// Submit the populated command list:
		std::shared_ptr<dx12::CommandList> commandLists[] =
		{
			commandList
		};
		directQueue.Execute(1, commandLists);
	}


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
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
