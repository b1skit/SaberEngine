// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "Context_DX12.h"
#include "DebugConfiguration.h"
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
#include "Debug_DX12.h"
#include "Shader_DX12.h"
#include "TextureTarget_DX12.h"
#include "SceneManager.h"
#include "Camera.h"
#include "CPUDescriptorHeapManager_DX12.h"
#include "MathUtils.h"
#include "GraphicsSystem_TempDebug.h"


// TEMP DEBUG CODE:
namespace
{
	using dx12::CheckHResult;


	// TODO: Move this to GraphicsSystem_TempDebug, and push it through as a batch
	static std::shared_ptr<re::MeshPrimitive> s_helloTriangle = nullptr;


	// TODO: Make this a platform function, and call it for all APIs during startup
	bool CreateAPIResources()
	{
		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::CommandQueue& copyQueue = ctxPlatParams->m_commandQueues[dx12::CommandList::Copy];

		std::shared_ptr<dx12::CommandList> copyCommandList = copyQueue.GetCreateCommandList();

		// Note: This internally create all of the vertex stream resources
		dx12::MeshPrimitive::Create(*s_helloTriangle, copyCommandList->GetD3DCommandList()); 

		// Execute command queue, and wait for it to be done (blocking)
		std::shared_ptr<dx12::CommandList> commandLists[] =
		{
			copyCommandList
		};

		uint64_t copyQueueFenceVal = copyQueue.Execute(1, commandLists);
		copyQueue.CPUWait(copyQueueFenceVal);

		// TODO: We should destroy the vertex stream intermediate HEAP_TYPE_UPLOAD resources now that the copy is done

		std::shared_ptr<re::Shader> k_helloShader = std::make_shared<re::Shader>("HelloTriangle");
		dx12::Shader::Create(*k_helloShader);

		s_helloTriangle->GetMeshMaterial()->SetShader(k_helloShader);


		dx12::SwapChain::PlatformParams const* swapChainParams = 
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();


		// Create a pipeline state:
		// TODO: We should be creating a library of these at startup
		gr::PipelineState defaultGrPipelineState{}; // Temp hax: Use a default gr::PipelineState

		dx12::Context::CreateAddPipelineState(
			defaultGrPipelineState,
			*k_helloShader, 
			*swapChainParams->m_backbufferTargetSets[0]);

		return true;
	}
}



namespace dx12
{
	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// TEMP DEBUG CODE: Need to have this created before CreateAPIResources
		s_helloTriangle = meshfactory::CreateHelloTriangle(10.f, -10.f);



		CreateAPIResources();

		renderManager.m_graphicsSystems.emplace_back(
			make_shared<gr::TempDebugGraphicsSystem>("DX12 Temp Debug Graphics System"));

	}


	void RenderManager::Render(re::RenderManager& renderManager)
	{
		re::Context const& context = re::RenderManager::Get()->GetContext();

		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::CommandQueue& directQueue = ctxPlatParams->m_commandQueues[dx12::CommandList::Direct];

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


		// Set the pipeline state and root signature first:
		commandList->SetPipelineState(*ctxPlatParams->m_pipelineState);
		commandList->SetGraphicsRootSignature(ctxPlatParams->m_pipelineState->GetRootSignature());

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
		dx12::CommandQueue& directQueue = ctxPlatParams->m_commandQueues[dx12::CommandList::Direct];

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
		ctxPlatParams->m_commandQueues[CommandList::CommandListType::Direct].Flush();
		ctxPlatParams->m_commandQueues[CommandList::CommandListType::Copy].Flush();

		// TEMP DEBUG CODE:
		s_helloTriangle = nullptr;
	}
}
