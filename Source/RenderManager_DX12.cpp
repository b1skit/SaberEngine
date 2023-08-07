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
	// TODO: Figure out why creation fails for D3D_FEATURE_LEVEL_12_2
	const D3D_FEATURE_LEVEL RenderManager::k_targetFeatureLevel = D3D_FEATURE_LEVEL_12_1;


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<gr::ComputeMipsGraphicsSystem>("DX12 Compute Mips Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<gr::TempDebugGraphicsSystem>("DX12 Temp Debug Graphics System"));
	}


	void RenderManager::CreateAPIResources()
	{
		re::Context const& context = GetContext();
		dx12::SwapChain::PlatformParams const* swapChainParams =
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();


		const bool hasDataToCopy = !m_newMeshPrimitives.m_newObjects.empty() ||
			!m_newTextures.m_newObjects.empty();

		// Handle anything that requires a copy queue:
		uint64_t copyQueueFenceVal = 0;
		std::vector<ComPtr<ID3D12Resource>> intermediateResources;
		dx12::CommandQueue* copyQueue = nullptr;
		if (hasDataToCopy)
		{
			copyQueue = &dx12::Context::GetCommandQueue(dx12::CommandListType::Copy);

			// TODO: Get multiple command lists, and record on multiple threads:
			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue->GetCreateCommandList();
			ID3D12GraphicsCommandList2* copyCommandListD3D = copyCommandList->GetD3DCommandList();

			// Mesh Primitives:
			if (!m_newMeshPrimitives.m_newObjects.empty())
			{
				std::lock_guard<std::mutex> lock(m_newMeshPrimitives.m_mutex);
				for (auto& newMeshPrimitive : m_newMeshPrimitives.m_newObjects)
				{
					dx12::MeshPrimitive::Create(*newMeshPrimitive.second, copyCommandListD3D, intermediateResources);
				}
				m_newMeshPrimitives.m_newObjects.clear();
			}

			// Textures:
			if (!m_newTextures.m_newObjects.empty())
			{
				gr::ComputeMipsGraphicsSystem* computeMipsGS = 
					GetGraphicsSystem<gr::ComputeMipsGraphicsSystem>();

				std::lock_guard<std::mutex> lock(m_newTextures.m_mutex);
				for (auto& texture : m_newTextures.m_newObjects)
				{
					dx12::Texture::Create(*texture.second, copyCommandListD3D, intermediateResources);

					if (texture.second->GetTextureParams().m_useMIPs)
					{
						computeMipsGS->AddTexture(texture.second);
					}
				}
				m_newTextures.m_newObjects.clear();
			}

			// Execute the copy before moving on
			copyQueueFenceVal = copyQueue->Execute(1, &copyCommandList);
		}

		// Samplers:
		if (!m_newSamplers.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newSamplers.m_mutex);
			for (auto& newObject : m_newSamplers.m_newObjects)
			{
				dx12::Sampler::Create(*newObject.second);
			}
			m_newSamplers.m_newObjects.clear();
		}
		// Texture Target Sets:
		if (!m_newTargetSets.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newTargetSets.m_mutex);
			for (auto& newObject : m_newTargetSets.m_newObjects)
			{
				dx12::TextureTargetSet::CreateColorTargets(*newObject.second);
				dx12::TextureTargetSet::CreateDepthStencilTarget(*newObject.second);
			}
			m_newTargetSets.m_newObjects.clear();
		}
		// Shaders:
		if (!m_newShaders.m_newObjects.empty())
		{
			SEAssert("Creating PSO's for DX12 Shaders requires a re::PipelineState from a RenderStage, but the "
				"pipeline is empty",
				!m_renderPipeline.GetStagePipeline().empty());

			std::lock_guard<std::mutex> lock(m_newShaders.m_mutex);
			for (auto& shader : m_newShaders.m_newObjects)
			{
				// Create the Shader object:
				dx12::Shader::Create(*shader.second);

				// Create any necessary PSO's for the Shader:
				for (re::StagePipeline& stagePipeline : m_renderPipeline.GetStagePipeline())
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
								stageTargets = dx12::SwapChain::GetBackBufferTargetSet(context.GetSwapChain());
							}

							dx12::Context::CreateAddPipelineState(
								*shader.second,
								renderStage->GetStagePipelineState(),								
								*stageTargets);
						}
					}
				}
			}
			m_newShaders.m_newObjects.clear();
		}
		// Parameter Blocks:
		if (!m_newParameterBlocks.m_newObjects.empty())
		{
			std::lock_guard<std::mutex> lock(m_newParameterBlocks.m_mutex);
			for (auto& newObject : m_newParameterBlocks.m_newObjects)
			{
				dx12::ParameterBlock::Create(*newObject.second);
			}
			m_newParameterBlocks.m_newObjects.clear();
		}

		// If we added anything to the copy queue, and wait for it to be done (blocking)
		// TODO: We should use the resource modification fence instead of waiting here. This would allow us to clear the
		// intermediateResources at the end of the frame/start of the next, and GPUWait on the copy instead
		if (copyQueue)
		{
			copyQueue->CPUWait(copyQueueFenceVal);
			intermediateResources.clear(); // The copy is done: Free the intermediate HEAP_TYPE_UPLOAD resources
		}
	}


	void RenderManager::Render()
	{
		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::CommandQueue& directQueue = dx12::Context::GetCommandQueue(dx12::CommandListType::Direct);
		dx12::CommandQueue& computeQueue = dx12::Context::GetCommandQueue(dx12::CommandListType::Compute);

		std::vector<std::shared_ptr<dx12::CommandList>> commandLists;

		// Render each stage:
		for (StagePipeline& stagePipeline : m_renderPipeline.GetStagePipeline())
		{
			// Generic lambda: Process stages from various pipelines
			auto ProcessRenderStage = [&](std::shared_ptr<re::RenderStage> renderStage)
			{
				// Note: Our command lists and associated command allocators are already closed/reset
				std::shared_ptr<dx12::CommandList> directCommandList = nullptr;
				std::shared_ptr<dx12::CommandList> computeCommandList = nullptr;

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
				if (stageTargets == nullptr)
				{
					SEAssert("Only the graphics queue/command lists can render to the backbuffer", 
						renderStage->GetStageType() == re::RenderStage::RenderStageType::Graphics);

					stageTargets = dx12::SwapChain::GetBackBufferTargetSet(GetContext().GetSwapChain());
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

					// Set per-frame stage textures/sampler inputs:
					for (auto const& texSamplerInput : renderStage->GetPerFrameTextureInputs())
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
				}
				break;
				case re::RenderStage::RenderStageType::Graphics:
				{
					// Bind our graphics stage render target(s) to the output merger (OM):
					currentCommandList->SetRenderTargets(*stageTargets);
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
					currentCommandList->ClearColorTargets(*stageTargets);
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
							// TODO: Support batch textures of any subresource/mip
							currentCommandList->SetTexture(
								std::get<0>(texSamplerInput),	// Shader name
								std::get<1>(texSamplerInput),	// Texture
								std::numeric_limits<uint32_t>::max());
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

				// We're done: We have a command list for everything that happened on the current StagePipeline
				if (computeCommandList != nullptr)
				{
					commandLists.emplace_back(computeCommandList);
				}
				if (directCommandList != nullptr)
				{
					commandLists.emplace_back(directCommandList);
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
		}

		// Command lists must be submitted on a single thread, and in the same order as the render stages they're
		// generated from to ensure modification fences and GPU waits are are handled correctly
		size_t startIdx = 0;
		while (startIdx < commandLists.size())
		{
			// TODO: Use the SE enums where possible, to make future porting/code reuse easier
			const D3D12_COMMAND_LIST_TYPE cmdListType = commandLists[startIdx]->GetD3DCommandListType();

			// Find the index of the last command list of the same type:
			size_t endIdx = startIdx + 1;
			while (endIdx < commandLists.size() &&
				commandLists[endIdx]->GetD3DCommandListType() == cmdListType)
			{
				endIdx++;
			}

			const size_t numCmdLists = endIdx - startIdx;

			switch (cmdListType)
			{
			case D3D12_COMMAND_LIST_TYPE_DIRECT:
			{
				directQueue.Execute(static_cast<uint32_t>(numCmdLists), &commandLists[startIdx]);
			}
			break;
			case D3D12_COMMAND_LIST_TYPE_BUNDLE:
			{
				SEAssertF("TODO: Support this type");
			}
			break;
			case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			{
				computeQueue.Execute(static_cast<uint32_t>(numCmdLists), &commandLists[startIdx]);
			}
			break;
			case D3D12_COMMAND_LIST_TYPE_COPY:
			{
				SEAssertF("Currently not expecting to find a copy queue genereted from a render stage");
			}
			case D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE:
			case D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS:
			case D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE:
			case D3D12_COMMAND_LIST_TYPE_NONE:
			{
				SEAssertF("TODO: Support this type");
			}
			break;
			default:
				SEAssertF("Invalid command list type");
			}

			startIdx = endIdx;
		}
	}


	void RenderManager::RenderImGui()
	{
		// Early out if there is nothing to draw:
		if (m_imGuiCommands.empty())
		{
			return;
		}

		// Start a new ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Process the queue of commands for the current frame:
		while (!m_imGuiCommands.empty())
		{
			m_imGuiCommands.front()->Execute();
			m_imGuiCommands.pop();
		}

		// ImGui internal rendering
		ImGui::Render(); // Note: Does not touch the GPU/graphics API

		// Get our SE rendering objects:
		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::CommandQueue& directQueue = dx12::Context::GetCommandQueue(dx12::CommandListType::Direct);
		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		// Draw directly to the swapchain backbuffer
		re::SwapChain const& swapChain = context.GetSwapChain();
		commandList->SetRenderTargets(*dx12::SwapChain::GetBackBufferTargetSet(swapChain));

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
		Context::GetCommandQueue(dx12::CommandListType::Copy).Flush();
		Context::GetCommandQueue(dx12::CommandListType::Direct).Flush();
	}
}
