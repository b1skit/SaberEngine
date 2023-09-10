// � 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

#include <pix3.h>

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "GraphicsSystem_ComputeMips.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_TempDebug.h"
#include "MeshPrimitive_DX12.h"
#include "ParameterBlock_DX12.h"
#include "RenderManager_DX12.h"
#include "RenderSystem.h"
#include "Sampler_DX12.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

using re::RenderStage;
using re::StagePipeline;
using Microsoft::WRL::ComPtr;
using glm::vec4;
using std::make_shared;
using std::shared_ptr;
using std::vector;


namespace dx12
{
	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// Create and add our RenderSystems:
		renderManager.m_renderSystems.emplace_back(re::RenderSystem::Create("Default DX12 RenderSystem"));
		re::RenderSystem* defaultRenderSystem = renderManager.m_renderSystems.back().get();

		// Build the default render system create pipeline:
		auto DefaultRenderSystemCreatePipeline = [](re::RenderSystem* defaultRS)
		{
			std::vector<std::shared_ptr<gr::GraphicsSystem>>& graphicsSystems = defaultRS->GetGraphicsSystems();

			// Create and add graphics systems:
			std::shared_ptr<gr::ComputeMipsGraphicsSystem> computeMipsGS =
				std::make_shared<gr::ComputeMipsGraphicsSystem>("DX12 Compute Mips Graphics System");
			graphicsSystems.emplace_back(computeMipsGS);

			std::shared_ptr<gr::GBufferGraphicsSystem> gbufferGS =
				std::make_shared<gr::GBufferGraphicsSystem>("DX12 GBuffer Graphics System");
			graphicsSystems.emplace_back(gbufferGS);

			//std::shared_ptr<gr::DeferredLightingGraphicsSystem> deferredLightingGS =
			//	std::make_shared<gr::DeferredLightingGraphicsSystem>("DX12 Deferred Lighting Graphics System");
			//graphicsSystems.emplace_back(deferredLighting);

			std::shared_ptr<gr::TempDebugGraphicsSystem> tempDebugGS =
				std::make_shared<gr::TempDebugGraphicsSystem>("DX12 Temp Debug Graphics System");
			graphicsSystems.emplace_back(tempDebugGS);

			// Build the creation pipeline:
			computeMipsGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(computeMipsGS->GetName()));
			gbufferGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(gbufferGS->GetName()));
			//deferredLightingGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(deferredLightingGS->GetName()));
			tempDebugGS->Create(defaultRS->GetRenderPipeline().AddNewStagePipeline(tempDebugGS->GetName()));
		};
		defaultRenderSystem->SetCreatePipeline(DefaultRenderSystemCreatePipeline);


		// Build the default render system update pipeline:
		auto UpdatePipeline = [](re::RenderSystem* renderSystem)
		{
			// Get our GraphicsSystems:
			gr::ComputeMipsGraphicsSystem* computeMipsGS = renderSystem->GetGraphicsSystem<gr::ComputeMipsGraphicsSystem>();
			gr::GBufferGraphicsSystem* gbufferGS = renderSystem->GetGraphicsSystem<gr::GBufferGraphicsSystem>();
			//gr::DeferredLightingGraphicsSystem* deferredLightingGS = renderSystem->GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();
			gr::TempDebugGraphicsSystem* tempDebugGS = renderSystem->GetGraphicsSystem<gr::TempDebugGraphicsSystem>();

			// Execute per-frame updates:
			computeMipsGS->PreRender();
			gbufferGS->PreRender();
			tempDebugGS->PreRender();
		};
		defaultRenderSystem->SetUpdatePipeline(UpdatePipeline);
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		// Note: We've already obtained the read lock on all new resources by this point

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		dx12::SwapChain::PlatformParams const* swapChainParams =
			context->GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		const bool hasDataToCopy = 
			renderManager.m_newMeshPrimitives.HasReadData() ||
			renderManager.m_newTextures.HasReadData();

		// Handle anything that requires a copy queue:
		uint64_t copyQueueFenceVal = 0;
		std::vector<ComPtr<ID3D12Resource>> intermediateResources;
		dx12::CommandQueue* copyQueue = nullptr;
		if (hasDataToCopy)
		{
			copyQueue = &context->GetCommandQueue(dx12::CommandListType::Copy);

			PIXBeginEvent(
				copyQueue->GetD3DCommandQueue(), 
				PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CopyQueue), 
				"Copy Queue: Create API Resources");

			// TODO: Get multiple command lists, and record on multiple threads:
			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();
			ID3D12GraphicsCommandList2* copyCommandListD3D = copyCommandList->GetD3DCommandList();

			// Mesh Primitives:
			if (renderManager.m_newMeshPrimitives.HasReadData())
			{
				for (auto& newMeshPrimitive : renderManager.m_newMeshPrimitives.Get())
				{
					dx12::MeshPrimitive::Create(*newMeshPrimitive.second, copyCommandListD3D, intermediateResources);
				}
			}

			// Textures:
			if (renderManager.m_newTextures.HasReadData())
			{
				for (auto& texture : renderManager.m_newTextures.Get())
				{
					dx12::Texture::Create(*texture.second, copyCommandListD3D, intermediateResources);
				}
			}

			// Execute the copy before moving on
			copyQueueFenceVal = copyQueue->Execute(1, &copyCommandList);
		}

		// Samplers:
		if (renderManager.m_newSamplers.HasReadData())
		{
			for (auto& newObject : renderManager.m_newSamplers.Get())
			{
				dx12::Sampler::Create(*newObject.second);
			}
		}
		// Texture Target Sets:
		if (renderManager.m_newTargetSets.HasReadData())
		{
			for (auto& newObject : renderManager.m_newTargetSets.Get())
			{
				newObject.second->Commit();
				dx12::TextureTargetSet::CreateColorTargets(*newObject.second);
				dx12::TextureTargetSet::CreateDepthStencilTarget(*newObject.second);
			}
		}
		// Shaders:
		if (renderManager.m_newShaders.HasReadData())
		{
			for (auto& shader : renderManager.m_newShaders.Get())
			{
				// Create the Shader object:
				dx12::Shader::Create(*shader.second);

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
								renderStage->GetStageShader()->GetNameID() == shader.second->GetNameID())
							{
								std::shared_ptr<re::TextureTargetSet const> stageTargets = renderStage->GetTextureTargetSet();
								if (!stageTargets)
								{
									stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain());
								}

								context->CreateAddPipelineState(
									*shader.second,
									renderStage->GetStagePipelineState(),
									*stageTargets);
							}
						}
					}
				}
			}
		}
		// Parameter Blocks:
		if (renderManager.m_newParameterBlocks.HasReadData())
		{
			for (auto& newObject : renderManager.m_newParameterBlocks.Get())
			{
				dx12::ParameterBlock::Create(*newObject.second);
			}
		}

		// If we added anything to the copy queue, and wait for it to be done (blocking)
		// TODO: We should use the resource modification fence instead of waiting here. This would allow us to clear the
		// intermediateResources at the end of the frame/start of the next, and GPUWait on the copy instead
		if (copyQueue)
		{
			copyQueue->CPUWait(copyQueueFenceVal);
			intermediateResources.clear(); // The copy is done: Free the intermediate HEAP_TYPE_UPLOAD resources

			PIXEndEvent(copyQueue->GetD3DCommandQueue());
		}
	}


	void RenderManager::Render()
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);
		dx12::CommandQueue& computeQueue = context->GetCommandQueue(dx12::CommandListType::Compute);

		std::vector<std::shared_ptr<dx12::CommandList>> commandLists;

		// Render each RenderSystem in turn:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			// Render each stage in the RenderSystem's RenderPipeline:
			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();
			for (StagePipeline& stagePipeline : renderPipeline.GetStagePipeline())
			{
				// Note: Our command lists and associated command allocators are already closed/reset
				std::shared_ptr<dx12::CommandList> directCommandList = nullptr;
				std::shared_ptr<dx12::CommandList> computeCommandList = nullptr;

				// Process all of the RenderStages attached to the StagePipeline:
				std::list<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
				for (std::shared_ptr<re::RenderStage> const& renderStage : stagePipeline.GetRenderStages())
				{
					// Skip empty stages:
					if (renderStage->GetStageBatches().empty())
					{
						continue;
					}

					// Get a CommandList for the current RenderStage:
					dx12::CommandList* currentCommandList = nullptr;
					switch (renderStage->GetStageType())
					{
					case re::RenderStage::RenderStageType::Graphics:
					{
						if (directCommandList == nullptr)
						{
							directCommandList = directQueue.GetCreateCommandList();

							PIXBeginEvent( // Add a PIX marker to wrap the StagePipeline:
								directCommandList->GetD3DCommandList(),
								PIX_COLOR_INDEX(PIX_FORMAT_COLOR::GraphicsCommandList),
								stagePipeline.GetName().c_str());
						}
						currentCommandList = directCommandList.get();
						PIXBeginEvent( // Add a PIX marker to wrap the RenderStage:
							currentCommandList->GetD3DCommandList(),
							PIX_COLOR_INDEX(PIX_FORMAT_COLOR::GraphicsCommandList),
							renderStage->GetName().c_str());
					}
					break;
					case re::RenderStage::RenderStageType::Compute:
					{
						if (computeCommandList == nullptr)
						{
							computeCommandList = computeQueue.GetCreateCommandList();

							PIXBeginEvent( // Add a PIX marker to wrap the StagePipeline:
								computeCommandList->GetD3DCommandList(),
								PIX_COLOR_INDEX(PIX_FORMAT_COLOR::GraphicsCommandList),
								stagePipeline.GetName().c_str());
						}
						currentCommandList = computeCommandList.get();
						PIXBeginEvent( // Add a PIX marker to wrap the RenderStage:
							currentCommandList->GetD3DCommandList(),
							PIX_COLOR_INDEX(PIX_FORMAT_COLOR::ComputeCommandList),
							renderStage->GetName().c_str());
					}
					break;
					default:
						SEAssertF("Invalid stage type");
					}

					gr::PipelineState const& pipelineState = renderStage->GetStagePipelineState();

					// Get the stage targets:
					std::shared_ptr<re::TextureTargetSet const> stageTargets = renderStage->GetTextureTargetSet();
					if (stageTargets == nullptr)
					{
						SEAssert("Only the graphics queue/command lists can render to the backbuffer",
							renderStage->GetStageType() == re::RenderStage::RenderStageType::Graphics);

						stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context->GetSwapChain());
					}

					auto SetDrawState = [&renderStage, &context](
						re::Shader const* shader,
						gr::PipelineState const& grPipelineState,
						re::TextureTargetSet const* targetSet,
						dx12::CommandList* commandList)
					{
						// Set the pipeline state and root signature first:
						std::shared_ptr<dx12::PipelineState> pso = context->GetPipelineStateObject(
							*shader,
							grPipelineState,
							targetSet);
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
						for (auto const& texSamplerInput : renderStage->GetTextureInputs())
						{
							commandList->SetTexture(
								texSamplerInput.m_shaderName, texSamplerInput.m_texture, texSamplerInput.m_subresource);
							// Note: Static samplers have already been set during root signature creation
						}
					};

					re::Shader* stageShader = renderStage->GetStageShader();
					const bool hasStageShader = stageShader != nullptr;

					// If we have a stage shader, we can set the stage PBs once for all batches
					if (hasStageShader)
					{
						SetDrawState(stageShader, pipelineState, stageTargets.get(), currentCommandList);
					}

					// Set targets, now that the pipeline is set
					switch (renderStage->GetStageType())
					{
					case re::RenderStage::RenderStageType::Compute:
					{
						currentCommandList->SetComputeTargets(*stageTargets);

						// TODO: Support compute target clearing (tricky: Need a copy of descriptors in the GPU-visible heap)
					}
					break;
					case re::RenderStage::RenderStageType::Graphics:
					{
						// Bind our graphics stage render target(s) to the output merger (OM):
						currentCommandList->SetRenderTargets(*stageTargets);

						// Clear the render targets:
						const gr::PipelineState::ClearTarget clearTargetMode = pipelineState.GetClearTarget();
						if (clearTargetMode == gr::PipelineState::ClearTarget::Color ||
							clearTargetMode == gr::PipelineState::ClearTarget::ColorDepth)
						{
							currentCommandList->ClearColorTargets(*stageTargets);
						}
						if (clearTargetMode == gr::PipelineState::ClearTarget::Depth ||
							clearTargetMode == gr::PipelineState::ClearTarget::ColorDepth)
						{
							currentCommandList->ClearDepthTarget(stageTargets->GetDepthStencilTarget());
						}
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

							SetDrawState(batchShader, pipelineState, stageTargets.get(), currentCommandList);
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
								currentCommandList->SetTexture(
									texSamplerInput.m_shaderName,
									texSamplerInput.m_texture,
									texSamplerInput.m_subresource);
								// Note: Static samplers have already been set during root signature creation
							}
						}

						switch (renderStage->GetStageType())
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

					// Close the PIX marker for the current stage:
					switch (renderStage->GetStageType())
					{
					case re::RenderStage::RenderStageType::Graphics:
					{
						PIXEndEvent(directCommandList->GetD3DCommandList());
					}
					break;
					case re::RenderStage::RenderStageType::Compute:
					{
						PIXEndEvent(computeCommandList->GetD3DCommandList());
					}
					break;
					default:
						SEAssertF("Invalid stage type");
					}
				}; // ProcessRenderStage


				// We're done: We've recorded a command list for the current StagePipeline
				if (computeCommandList != nullptr)
				{
					commandLists.emplace_back(computeCommandList);
					PIXEndEvent(computeCommandList->GetD3DCommandList());

				}
				if (directCommandList != nullptr)
				{
					commandLists.emplace_back(directCommandList);
					PIXEndEvent(directCommandList->GetD3DCommandList());
				}
			} // StagePipeline loop
		}


		// Command lists must be submitted on a single thread, and in the same order as the render stages they're
		// generated from to ensure modification fences and GPU waits are are handled correctly
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

			const size_t numCmdLists = endIdx - startIdx;

			switch (cmdListType)
			{
			case CommandListType::Direct:
			{
				PIXBeginEvent(directQueue.GetD3DCommandQueue(),
					PIX_COLOR_INDEX(PIX_FORMAT_COLOR::GraphicsQueue),
					"Direct command queue");
				
				directQueue.Execute(static_cast<uint32_t>(numCmdLists), &commandLists[startIdx]);

				PIXEndEvent(directQueue.GetD3DCommandQueue());
			}
			break;
			case CommandListType::Bundle:
			{
				SEAssertF("TODO: Support this type");
			}
			break;
			case CommandListType::Compute:
			{
				PIXBeginEvent(computeQueue.GetD3DCommandQueue(),
					PIX_COLOR_INDEX(PIX_FORMAT_COLOR::ComputeQueue),
					"Compute command queue");

				computeQueue.Execute(static_cast<uint32_t>(numCmdLists), &commandLists[startIdx]);

				PIXEndEvent(computeQueue.GetD3DCommandQueue());
			}
			break;
			case CommandListType::Copy:
			{
				SEAssertF("Currently not expecting to find a copy queue genereted from a render stage");
			}
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
		}
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
		commandList->SetRenderTargets(*dx12::SwapChain::GetBackBufferTargetSet(swapChain));

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
