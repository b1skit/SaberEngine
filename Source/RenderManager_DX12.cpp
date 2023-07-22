// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "GraphicsSystem_ComputeMips.h"
#include "GraphicsSystem_TempDebug.h"
#include "MeshPrimitive_DX12.h"
#include "ParameterBlock_DX12.h"
#include "RenderManager_DX12.h"
#include "Sampler_DX12.h"
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
	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<gr::ComputeMipsGraphicsSystem>("DX12 Compute Mips Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<gr::TempDebugGraphicsSystem>("DX12 Temp Debug Graphics System"));
	}


	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		re::Context const& context = renderManager.GetContext();
		dx12::SwapChain::PlatformParams const* swapChainParams =
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();


		const bool hasDataToCopy = !renderManager.m_newMeshPrimitives.m_newObjects.empty() ||
			!renderManager.m_newTextures.m_newObjects.empty();

		// Handle anything that requires a copy queue:
		if (hasDataToCopy)
		{
			// TODO: Get multiple command lists, and record on multiple threads:
			dx12::CommandQueue& copyQueue = dx12::Context::GetCommandQueue(dx12::CommandList::CommandListType::Copy);
			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue.GetCreateCommandList();
			ID3D12GraphicsCommandList2* copyCommandListD3D = copyCommandList->GetD3DCommandList();

			std::vector<ComPtr<ID3D12Resource>> intermediateResources;

			// Mesh Primitives:
			if (!renderManager.m_newMeshPrimitives.m_newObjects.empty())
			{
				std::lock_guard<std::mutex> lock(renderManager.m_newMeshPrimitives.m_mutex);
				for (auto& newMeshPrimitive : renderManager.m_newMeshPrimitives.m_newObjects)
				{
					dx12::MeshPrimitive::Create(*newMeshPrimitive.second, copyCommandListD3D, intermediateResources);
				}
				renderManager.m_newMeshPrimitives.m_newObjects.clear();
			}

			// Textures:
			if (!renderManager.m_newTextures.m_newObjects.empty())
			{
				gr::ComputeMipsGraphicsSystem* computeMipsGS = 
					renderManager.GetGraphicsSystem<gr::ComputeMipsGraphicsSystem>();

				std::lock_guard<std::mutex> lock(renderManager.m_newTextures.m_mutex);
				for (auto& texture : renderManager.m_newTextures.m_newObjects)
				{
					dx12::Texture::Create(*texture.second, copyCommandListD3D, intermediateResources);

					if (texture.second->GetTextureParams().m_useMIPs)
					{
						computeMipsGS->AddTexture(texture.second);
					}
				}
				renderManager.m_newTextures.m_newObjects.clear();
			}


			// Execute command queue, and wait for it to be done (blocking)
			std::shared_ptr<dx12::CommandList> commandLists[] =
			{
				copyCommandList
			};
			uint64_t copyQueueFenceVal = copyQueue.Execute(1, commandLists);
			copyQueue.CPUWait(copyQueueFenceVal);

			// The copy is done: Free the intermediate HEAP_TYPE_UPLOAD resources
			intermediateResources.clear();

			// TODO: We should clear the intermediateResources at the end of the frame, and GPUWait on the copy instead
			// -> We should also progress further in the resource creation instead of waiting here
		}

		// Samplers:
		if (!renderManager.m_newSamplers.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(renderManager.m_newSamplers.m_mutex);
			for (auto& newObject : renderManager.m_newSamplers.m_newObjects)
			{
				dx12::Sampler::Create(*newObject.second);
			}
			renderManager.m_newSamplers.m_newObjects.clear();
		}
		// Texture Target Sets:
		if (!renderManager.m_newTargetSets.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(renderManager.m_newTargetSets.m_mutex);
			for (auto& newObject : renderManager.m_newTargetSets.m_newObjects)
			{
				dx12::TextureTargetSet::CreateColorTargets(*newObject.second);
				dx12::TextureTargetSet::CreateDepthStencilTarget(*newObject.second);
			}
			renderManager.m_newTargetSets.m_newObjects.clear();
		}
		// Shaders:
		if (!renderManager.m_newShaders.m_newObjects.empty())
		{
			SEAssert("Creating PSO's for DX12 Shaders requires a re::PipelineState from a RenderStage, but the "
				"pipeline is empty",
				!renderManager.m_renderPipeline.GetStagePipeline().empty());

			std::lock_guard<std::mutex> lock(renderManager.m_newShaders.m_mutex);
			for (auto& shader : renderManager.m_newShaders.m_newObjects)
			{
				// Create the Shader object:
				dx12::Shader::Create(*shader.second);

				// Create any necessary PSO's for the Shader:
				for (re::StagePipeline& stagePipeline : renderManager.m_renderPipeline.GetStagePipeline())
				{
					std::vector<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
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
								// We (currently) assume a null TextureTargetSet indicates the backbuffer is the target
								stageTargets = swapChainParams->m_backbufferTargetSet;
							}

							dx12::Context::CreateAddPipelineState(
								*shader.second,
								renderStage->GetStagePipelineState(),								
								*stageTargets);
						}
					}
				}
			}
			renderManager.m_newShaders.m_newObjects.clear();
		}
		// Parameter Blocks:
		if (!renderManager.m_newParameterBlocks.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(renderManager.m_newParameterBlocks.m_mutex);
			for (auto& newObject : renderManager.m_newParameterBlocks.m_newObjects)
			{
				dx12::ParameterBlock::Create(*newObject.second);
			}
			renderManager.m_newParameterBlocks.m_newObjects.clear();
		}
	}


	void RenderManager::Render(re::RenderManager& renderManager)
	{
		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::CommandQueue& directQueue = dx12::Context::GetCommandQueue(dx12::CommandList::CommandListType::Direct);

		std::vector<std::shared_ptr<dx12::CommandList>> directCommandLists;
		directCommandLists.reserve(renderManager.m_renderPipeline.GetStagePipeline().size());

		dx12::CommandQueue& computeQueue = dx12::Context::GetCommandQueue(dx12::CommandList::CommandListType::Compute);

		std::vector<std::shared_ptr<dx12::CommandList>> computeCommandLists;
		computeCommandLists.reserve(renderManager.m_renderPipeline.GetStagePipeline().size());


		// Render each stage:
		for (StagePipeline& stagePipeline : renderManager.m_renderPipeline.GetStagePipeline())
		{
			// Note: Our command lists and associated command allocators are already closed/reset
			std::shared_ptr<dx12::CommandList> directCommandList = nullptr;
			std::shared_ptr<dx12::CommandList> computeCommandList = nullptr;
			

			// Generic lambda: Process stages from various pipelines
			auto ProcessRenderStage = [&](std::shared_ptr<re::RenderStage> renderStage)
			{
				dx12::CommandList* currentCommandList = nullptr;
				switch (renderStage->GetStageType())
				{
				case re::RenderStage::RenderStageType::Graphics:
				{
					if (directCommandList == nullptr)
					{
						directCommandList = directQueue.GetCreateCommandList();
					}
					currentCommandList = directCommandList.get();
				}
				break;
				case re::RenderStage::RenderStageType::Compute:
				{
					if (computeCommandList == nullptr)
					{
						computeCommandList = computeQueue.GetCreateCommandList();
					}
					currentCommandList = computeCommandList.get();
				}
				break;
				default:
					SEAssertF("Invalid stage type");
				}

				// TODO: Why can't this be a const& ?
				gr::PipelineState& pipelineState = renderStage->GetStagePipelineState();

				// Attach the stage targets, and transition the resources:
				std::shared_ptr<re::TextureTargetSet const> stageTargets = renderStage->GetTextureTargetSet();
				const bool isBackbufferTarget = stageTargets == nullptr;
				if (isBackbufferTarget)
				{
					SEAssert("Only the graphics queue/command lists can render to the backbuffer", 
						renderStage->GetStageType() == re::RenderStage::RenderStageType::Graphics);

					dx12::SwapChain::PlatformParams* swapChainParams =
						renderManager.GetContext().GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();
					SEAssert("Swap chain params and backbuffer cannot be null",
						swapChainParams && swapChainParams->m_backbufferTargetSet);

					stageTargets = swapChainParams->m_backbufferTargetSet; // Draw directly to the swapchain backbuffer
				}
				
				auto SetDrawState = [&renderStage](
					re::Shader const* shader, 
					gr::PipelineState& grPipelineState,
					re::TextureTargetSet const* targetSet,
					dx12::CommandList* commandList)
				{
					// Set the pipeline state and root signature first:
					std::shared_ptr<dx12::PipelineState> pso = dx12::Context::GetPipelineStateObject(
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
				}
				break;
				case re::RenderStage::RenderStageType::Graphics:
				{
					// Bind our graphics stage render target(s) to the output merger (OM):
					if (isBackbufferTarget)
					{
						currentCommandList->SetBackbufferRenderTarget();
					}
					else
					{
						currentCommandList->SetRenderTargets(*stageTargets);
					}

					// Set the viewport and scissor rectangles:
					currentCommandList->SetViewport(*stageTargets);
					currentCommandList->SetScissorRect(*stageTargets);
					// TODO: Should the viewport and scissor rects be set while we're setting the targets?
				}
				break;
				default:
					SEAssertF("Invalid stage type");
				}

				// Clear the render targets:
				// TODO: These should be per-target, to allow different outputs when using MRTs
				const gr::PipelineState::ClearTarget clearTargetMode = pipelineState.GetClearTarget();
				if (clearTargetMode == gr::PipelineState::ClearTarget::Color ||
					clearTargetMode == gr::PipelineState::ClearTarget::ColorDepth)
				{
					if (isBackbufferTarget)
					{
						const uint8_t backbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());
						currentCommandList->ClearColorTarget(stageTargets->GetColorTarget(backbufferIdx));
					}
					else
					{
						currentCommandList->ClearColorTargets(*stageTargets);
					}
				}
				if (clearTargetMode == gr::PipelineState::ClearTarget::Depth ||
					clearTargetMode == gr::PipelineState::ClearTarget::ColorDepth)
				{
					currentCommandList->ClearDepthTarget(stageTargets->GetDepthStencilTarget());
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
					if (renderStage->GetStagePipelineState().WritesColor())
					{
						for (auto const& texSamplerInput : batches[batchIdx].GetTextureAndSamplerInputs())
						{
							currentCommandList->SetTexture(
								std::get<0>(texSamplerInput),	// Shader name
								std::get<1>(texSamplerInput));	// Texture
							// Note: Static samplers have already been set during root signature creation
						}
					}

					switch (renderStage->GetStageType())
					{
					case re::RenderStage::RenderStageType::Graphics:
					{
						// Set the geometry for the draw:
						dx12::MeshPrimitive::PlatformParams* meshPrimPlatParams =
							batches[batchIdx].GetMeshPrimitive()->GetPlatformParams()->As<dx12::MeshPrimitive::PlatformParams*>();

						// TODO: Batches should contain the draw mode, instead of carrying around a MeshPrimitive
						currentCommandList->SetPrimitiveType(meshPrimPlatParams->m_drawMode);
						currentCommandList->SetVertexBuffers(batches[batchIdx].GetMeshPrimitive()->GetVertexStreams());

						dx12::VertexStream::PlatformParams_Index* indexPlatformParams =
							batches[batchIdx].GetMeshPrimitive()->GetVertexStream(
								re::MeshPrimitive::Indexes)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Index*>();
						currentCommandList->SetIndexBuffer(&indexPlatformParams->m_indexBufferView);

						// Record the draw:
						currentCommandList->DrawIndexedInstanced(
							batches[batchIdx].GetMeshPrimitive()->GetVertexStream(re::MeshPrimitive::Indexes)->GetNumElements(),
							static_cast<uint32_t>(batches[batchIdx].GetInstanceCount()),	// Instance count
							0,	// Start index location
							0,	// Base vertex location
							0);	// Start instance location
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
			}; // ProcessRenderStage


			// Single frame render stages:
			vector<std::shared_ptr<re::RenderStage>> const& singleFrameRenderStages = 
				stagePipeline.GetSingleFrameRenderStages();
			for (size_t stageIdx = 0; stageIdx < singleFrameRenderStages.size(); stageIdx++)
			{
				ProcessRenderStage(singleFrameRenderStages[stageIdx]);
			}

			// Render stages:
			vector<std::shared_ptr<re::RenderStage>> const& renderStages = stagePipeline.GetRenderStages();
			for (size_t stageIdx = 0; stageIdx < renderStages.size(); stageIdx++)
			{
				ProcessRenderStage(renderStages[stageIdx]);
			}

			// We're done: We have a command list for everything that happened on the current StagePipeline
			if (computeCommandList != nullptr)
			{
				computeCommandLists.emplace_back(computeCommandList);
			}
			if (directCommandList != nullptr)
			{
				directCommandLists.emplace_back(directCommandList);
			}
		}
		
		// Execute the command lists
		if (!computeCommandLists.empty())
		{
			computeQueue.Execute(static_cast<uint32_t>(computeCommandLists.size()), computeCommandLists.data());
		}
		if (!directCommandLists.empty())
		{
			directQueue.Execute(static_cast<uint32_t>(directCommandLists.size()), directCommandLists.data());
		}
	}


	void RenderManager::RenderImGui(re::RenderManager& renderManager)
	{
		// Early out if there is nothing to draw:
		if (renderManager.m_imGuiCommands.empty())
		{
			return;
		}

		// Start a new ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Process the queue of commands for the current frame:
		while (!renderManager.m_imGuiCommands.empty())
		{
			renderManager.m_imGuiCommands.front()->Execute();
			renderManager.m_imGuiCommands.pop();
		}

		// ImGui internal rendering
		ImGui::Render(); // Note: Does not touch the GPU/graphics API

		// Get our SE rendering objects:
		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		dx12::SwapChain::PlatformParams const* swapChainParams =
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		dx12::CommandQueue& directQueue = dx12::Context::GetCommandQueue(dx12::CommandList::CommandListType::Direct);
		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		commandList->SetBackbufferRenderTarget();

		// Configure the descriptor heap:
		ID3D12GraphicsCommandList2* d3dCommandList = commandList->GetD3DCommandList();

		ID3D12DescriptorHeap* descriptorHeaps[1] = { ctxPlatParams->m_imGuiGPUVisibleSRVDescriptorHeap.Get() };
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
		#pragma message("TODO: Implement dx12::RenderManager::Shutdown")
		LOG_ERROR("TODO: Implement dx12::RenderManager::Shutdown");

		// TODO: We should be able to iterate over all of these, but some of them aren't initialized
		// TODO: We also flush these in the context as well... But it's necessary here, since we delete objects next
		Context::GetCommandQueue(dx12::CommandList::CommandListType::Copy).Flush();
		Context::GetCommandQueue(dx12::CommandList::CommandListType::Direct).Flush();
	}
}
