// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "ParameterBlock_DX12.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture_DX12.h"


using Microsoft::WRL::ComPtr;
using glm::vec4;
using std::make_shared;
using std::shared_ptr;


// TEMP DEBUG CODE:
#include "MeshPrimitive_DX12.h"
#include "VertexStream_DX12.h"
#include "Shader_DX12.h"
#include "TextureTarget_DX12.h"
#include "SceneManager.h"
#include "Camera.h"
#include "GraphicsSystem_TempDebug.h"


// TEMP DEBUG CODE:
namespace
{
	using dx12::CheckHResult;

	// TODO: Move this to GraphicsSystem_TempDebug, and push it through as a batch
	static std::shared_ptr<re::MeshPrimitive> s_helloTriangle = nullptr;


	bool CreateDebugAPIResources()
	{
		// TODO: Move all of this to the debug GS

		s_helloTriangle = meshfactory::CreateHelloTriangle(10.f, -10.f); // Must be created before CreateDebugAPIResources

		std::shared_ptr<re::Shader> helloShader = re::Shader::Create("HelloTriangle");

		s_helloTriangle->GetMeshMaterial()->SetShader(helloShader);

		return true;
	}
}


namespace dx12
{
	void RenderManager::CreateAPIResources(re::RenderManager& renderManager)
	{
		re::Context const& context = renderManager.GetContext();
		dx12::SwapChain::PlatformParams const* swapChainParams =
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		// Shaders:
		if (!renderManager.m_newShaders.m_newObjects.empty())
		{
			SEAssert("Creating PSO's for DX12 Shaders requires a re::PipelineState from a RenderStage, but the "
				"pipeline is empty",
				!renderManager.m_pipeline.GetPipeline().empty());



			SEAssert("TEMP HAX: WE'RE ASSUMING OUR ONE DEBUG SHADER IS BEING CREATED HERE", 
				renderManager.m_newShaders.m_newObjects.size() == 1); 

			

			std::lock_guard<std::mutex> lock(renderManager.m_newShaders.m_mutex);
			for (auto& shader : renderManager.m_newShaders.m_newObjects)
			{
				// Create the Shader object:
				dx12::Shader::Create(*shader.second);

				// Create any necessary PSO's for the Shader:
				for (re::StagePipeline& stagePipeline : renderManager.m_pipeline.GetPipeline())
				{
					std::vector<re::RenderStage*> const& renderStages = stagePipeline.GetRenderStages();
					for (re::RenderStage* renderStage : renderStages)
					{
						// We assume either a RenderStage has a shader, or all batches rendered on a RenderStage will
						// have their own shader. So, we must create a PSO per Shader for each RenderStage with a null
						// Shader (as any Shader might be used there), or if the Shader is used by the RenderStage
						if (renderStage->GetStageShader() == nullptr ||
							renderStage->GetStageShader()->GetNameID() == shader.second->GetNameID())
						{
							std::shared_ptr<re::TextureTargetSet> stageTargets = renderStage->GetTextureTargetSet();
							if (!stageTargets)
							{
								// We (currently) assume a null TextureTargetSet indicates the backbuffer is the target
								stageTargets = swapChainParams->m_backbufferTargetSets[0];
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

		const bool hasDataToCopy = !renderManager.m_newMeshPrimitives.m_newObjects.empty();

		// Handle anything that requires a copy queue:
		if (hasDataToCopy)
		{
			// TODO: Get multiple command lists, and record on multiple threads:
			dx12::CommandQueue& copyQueue = dx12::Context::GetCommandQueue(dx12::CommandList::CommandListType::Copy);
			std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue.GetCreateCommandList();

			std::vector<ComPtr<ID3D12Resource>> intermediateResources;

			// Mesh Primitives:
			if (!renderManager.m_newMeshPrimitives.m_newObjects.empty())
			{
				std::lock_guard<std::mutex> lock(renderManager.m_newMeshPrimitives.m_mutex);
				for (auto& newObject : renderManager.m_newMeshPrimitives.m_newObjects)
				{
					dx12::MeshPrimitive::Create(
						*newObject.second, copyCommandList->GetD3DCommandList(), intermediateResources);
				}
				renderManager.m_newMeshPrimitives.m_newObjects.clear();
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
		}
	}


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		renderManager.m_graphicsSystems.emplace_back(
			make_shared<gr::TempDebugGraphicsSystem>("DX12 Temp Debug Graphics System"));

		// TODO: Remove this from RenderManager::Initialize
		CreateDebugAPIResources();
	}


	void RenderManager::Render(re::RenderManager& renderManager)
	{
		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::CommandQueue& directQueue = dx12::Context::GetCommandQueue(dx12::CommandList::CommandListType::Direct);

		// Note: Our command lists and associated command allocators are already closed/reset
		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		const uint8_t backbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());
		
		// Transition the backbuffer to the render target state:
		Microsoft::WRL::ComPtr<ID3D12Resource> backbufferResource =
			dx12::SwapChain::GetBackBufferResource(context.GetSwapChain());

		commandList->TransitionResource(			
			backbufferResource.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);


		// Clear the render targets:
		dx12::SwapChain::PlatformParams* swapChainParams =
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();
		
		// The swapchain requires contiguous RTV descriptors allocated in the same heap; compute the current one:
		// TODO: Stage CPU descriptor handles into GPU-visible descriptor heap, and pack into descriptor tables
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(swapChainParams->m_backbufferRTVDescriptors.GetBaseDescriptor());
		rtvHandle.Offset(swapChainParams->m_backbufferRTVDescriptors.GetDescriptorSize() * backbufferIdx); // TODO: The CD3DX12_CPU_DESCRIPTOR_HANDLE copy ctor does the offset automatically????

		// Debug: Vary the clear color to easily verify things are working
		auto now = std::chrono::system_clock::now().time_since_epoch();
		size_t seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
		const float scale = static_cast<float>((glm::sin(seconds) + 1.0) / 2.0);

		const vec4 clearColor = vec4(0.38f, 0.36f, 0.1f, 1.0f) * scale;

		dx12::Texture::PlatformParams* renderTargetPlatParams =
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->GetColorTarget(0).GetTexture()
				->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		commandList->ClearRTV(rtvHandle, clearColor); // TODO: This should just take the Target object

		// Clear depth target:
		dx12::Texture::PlatformParams* depthPlatParams =
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->GetDepthStencilTarget().GetTexture()
				->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// TODO: Stage CPU descriptor handles into GPU-visible descriptor heap, and pack into descriptor tables
		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptor = depthPlatParams->m_descriptor.GetBaseDescriptor();
		commandList->ClearDepth(dsvDescriptor, 1.f); // TODO: This should just take the Target object

		
		// Bind our render target(s) to the output merger (OM):
		commandList->SetRenderTargets(1, &rtvHandle, false, &dsvDescriptor);


		// TODO: Switch to stages, use the pipeline state actually set in a stage!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		gr::PipelineState tempDebugHaxDefaultPipelineState;
		

		// Set the pipeline state and root signature first:
		std::shared_ptr<dx12::PipelineState> pso = dx12::Context::GetPipelineStateObject(
				*s_helloTriangle->GetMeshMaterial()->GetShader(),
				tempDebugHaxDefaultPipelineState,
				swapChainParams->m_backbufferTargetSets[backbufferIdx].get());

		commandList->SetPipelineState(*pso);

		commandList->SetGraphicsRootSignature(pso->GetRootSignature());

		// TODO: Command list should have a SetViewport/SetScissorRect function that takes a TextureTargetSet. We should
		// not be passing command lists around
		dx12::TextureTargetSet::SetViewport(
			*swapChainParams->m_backbufferTargetSets[backbufferIdx], commandList->GetD3DCommandList());
		dx12::TextureTargetSet::SetScissorRect(
			*swapChainParams->m_backbufferTargetSets[backbufferIdx], commandList->GetD3DCommandList());


		dx12::MeshPrimitive::PlatformParams* meshPrimPlatParams = 
			s_helloTriangle->GetPlatformParams()->As<dx12::MeshPrimitive::PlatformParams*>();

		// TODO: Batches should contain the draw mode, instead of carrying around a MeshPrimitive
		commandList->SetPrimitiveType(meshPrimPlatParams->m_drawMode);
		
		commandList->SetVertexBuffers(s_helloTriangle->GetVertexStreams());


		dx12::VertexStream::PlatformParams_Index* indexPlatformParams =
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::Indexes)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Index*>();

		commandList->SetIndexBuffer(&indexPlatformParams->m_indexBufferView);




		// Bind parameter blocks:
		std::shared_ptr<gr::Camera> mainCam = en::SceneManager::GetSceneData()->GetMainCamera();
		
		commandList->SetParameterBlock(mainCam->GetCameraParams().get());

		commandList->CommitGPUDescriptors(); // Must be done before the draw command


		// TODO: Command list should have a wrapper for draw calls
		// -> Internally, call GetGPUDescriptorHeap()->Commit() etc
		commandList->DrawIndexedInstanced(
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::Indexes)->GetNumElements(),
			1,	// Instance count
			0,	// Start index location
			0,	// Base vertex location
			0);	// Start instance location


		std::shared_ptr<dx12::CommandList> commandLists[] =
		{
			commandList
		};

		// Record our last fence value, so we can add a GPU wait before transitioning the backbuffer for presentation
		directQueue.Execute(1, commandLists);
		// TODO: Should this value be tracked by the command queue?
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

		// Configure the render target:
		const uint8_t backbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(swapChainParams->m_backbufferRTVDescriptors.GetBaseDescriptor());
		rtvHandle.Offset(swapChainParams->m_backbufferRTVDescriptors.GetDescriptorSize() * backbufferIdx);

		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		commandList->SetRenderTargets(1, &rtvHandle, false, nullptr);

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

		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		// TODO: We should be able to iterate over all of these, but some of them aren't initialized
		// TODO: We also flush these in the context as well... But it's necessary here, since we delete objects next
		Context::GetCommandQueue(dx12::CommandList::CommandListType::Copy).Flush();
		Context::GetCommandQueue(dx12::CommandList::CommandListType::Direct).Flush();


		// TEMP DEBUG CODE:
		s_helloTriangle = nullptr;
	}
}
