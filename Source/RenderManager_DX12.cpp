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
		copyQueue.WaitForGPU(copyQueueFenceVal);

		// TODO: We should destroy the vertex stream intermediate HEAP_TYPE_UPLOAD resources now that the copy is done

		std::shared_ptr<re::Shader> k_helloShader = std::make_shared<re::Shader>("HelloTriangle");
		dx12::Shader::Create(*k_helloShader);

		s_helloTriangle->GetMeshMaterial()->SetShader(k_helloShader);


		dx12::SwapChain::PlatformParams const* swapChainParams = 
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();


		// Create a pipeline state:
		// TODO: We should be creating a library of these at startup
		dx12::TextureTargetSet::PlatformParams* swapChainTargetSetPlatParams = 
			swapChainParams->m_backbufferTargetSets[0]->GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

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
		rtvHandle.Offset(swapChainParams->m_backbufferRTVDescriptors.GetDescriptorSize() * backbufferIdx);

		// Debug: Vary the clear color to easily verify things are working
		auto now = std::chrono::system_clock::now().time_since_epoch();
		size_t seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
		const float scale = static_cast<float>((glm::sin(seconds) + 1.0) / 2.0);

		const vec4 clearColor = vec4(0.38f, 0.36f, 0.1f, 1.0f) * scale;

		dx12::Texture::PlatformParams* renderTargetPlatParams =
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->GetColorTarget(0).GetTexture()
				->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetView(
			rtvHandle,
			backbufferIdx,
			renderTargetPlatParams->m_descriptor.GetDescriptorSize());

		commandList->ClearRTV(renderTargetView, clearColor);


		// Clear depth target:
		dx12::Texture::PlatformParams* depthPlatParams =
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->GetDepthStencilTarget().GetTexture()
				->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// TODO: Stage CPU descriptor handles into GPU-visible descriptor heap, and pack into descriptor tables
		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptor = depthPlatParams->m_descriptor.GetBaseDescriptor();
		commandList->ClearDepth(dsvDescriptor, 1.f);

		
		// Bind our render target(s) to the output merger (OM):
		commandList->SetRenderTargets(1, &renderTargetView, false, &dsvDescriptor);


		// Set the pipeline state:
		commandList->SetPipelineState(ctxPlatParams->m_pipelineState->GetD3DPipelineState());
		commandList->SetGraphicsRootSignature(ctxPlatParams->m_pipelineState->GetRootSignature());

		dx12::TextureTargetSet::SetViewport(
			*swapChainParams->m_backbufferTargetSets[backbufferIdx], commandList->GetD3DCommandList());
		dx12::TextureTargetSet::SetScissorRect(
			*swapChainParams->m_backbufferTargetSets[backbufferIdx], commandList->GetD3DCommandList());


		// TODO: This should be set by a batch, w.r.t MeshPrimitive::Drawmode
		commandList->SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// TEMP HAX: Get the vertex buffer views:
		dx12::VertexStream::PlatformParams_Vertex* positionPlatformParams = 
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::Position)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>();

		dx12::VertexStream::PlatformParams_Vertex* normalPlatformParams = 
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::Normal)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>();

		dx12::VertexStream::PlatformParams_Vertex* tangentPlatformParams =
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::Tangent)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>();

		dx12::VertexStream::PlatformParams_Vertex* uv0PlatformParams =
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::UV0)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>();

		dx12::VertexStream::PlatformParams_Vertex* colorPlatformParams =
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::Color)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>();

		// Note: We could set these in a single call, if we're ok with using sequential slots
		commandList->SetVertexBuffers(re::MeshPrimitive::Position, 1, &positionPlatformParams->m_vertexBufferView);
		commandList->SetVertexBuffers(re::MeshPrimitive::Normal, 1, &normalPlatformParams->m_vertexBufferView);
		commandList->SetVertexBuffers(re::MeshPrimitive::Tangent, 1, &tangentPlatformParams->m_vertexBufferView);
		commandList->SetVertexBuffers(re::MeshPrimitive::UV0, 1, &uv0PlatformParams->m_vertexBufferView);
		commandList->SetVertexBuffers(re::MeshPrimitive::Color, 1, &colorPlatformParams->m_vertexBufferView);

		dx12::VertexStream::PlatformParams_Index* indexPlatformParams =
			s_helloTriangle->GetVertexStream(re::MeshPrimitive::Indexes)->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Index*>();

		commandList->SetIndexBuffer(&indexPlatformParams->m_indexBufferView);




		// TODO: Automatically bind parameter blocks
		// -> We need to be able to automatically set PBs to the correct locations in our descriptor tables
		dx12::ParameterBlock::PlatformParams* cameraPlatParams = en::SceneManager::GetSceneData()->GetMainCamera()
			->GetCameraParams()->GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();


		// TODO: The command list should wrap the GPU Descriptor Heap
		commandList->GetGPUDescriptorHeap()->SetDescriptorTable(
			0,															// Root param idx
			cameraPlatParams->m_cpuDescAllocation.GetBaseDescriptor(),	// D3D12_CPU_DESCRIPTOR_HANDLE
			0,															// offset
			1);															// count
		commandList->GetGPUDescriptorHeap()->Commit(); // Must be done before the draw command



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
		ctxPlatParams->m_lastFenceBeforePresent = directQueue.Execute(1, commandLists);
		// TODO: Should this value be tracked by the command queue?
	}


	void RenderManager::RenderImGui(re::RenderManager& renderManager)
	{
		#pragma message("TODO: Implement dx12::RenderManager::RenderImGui")

		//// Start the Dear ImGui frame
		//ImGui_ImplDX12_NewFrame();
		//ImGui_ImplWin32_NewFrame();
		//ImGui::NewFrame();

		// Process the queue of commands for the current frame:
		while (!renderManager.m_imGuiCommands.empty())
		{
			//renderManager.m_imGuiCommands.front()->Execute();
			renderManager.m_imGuiCommands.pop();
		}

		//// Rendering
		//ImGui::Render();

		//FrameContext* frameCtx = WaitForNextFrameResources();
		//UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
		//frameCtx->CommandAllocator->Reset();

		//D3D12_RESOURCE_BARRIER barrier = {};
		//barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		//barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		//barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
		//barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//g_pd3dCommandList->Reset(frameCtx->CommandAllocator, NULL);
		//g_pd3dCommandList->ResourceBarrier(1, &barrier);

		//// Render Dear ImGui graphics
		//const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		//g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, NULL);
		//g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, NULL);
		//g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
		//ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
		//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		//g_pd3dCommandList->ResourceBarrier(1, &barrier);
		//g_pd3dCommandList->Close();

		//g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_pd3dCommandList);

		//g_pSwapChain->Present(1, 0); // Present with vsync
		////g_pSwapChain->Present(0, 0); // Present without vsync

		//UINT64 fenceValue = g_fenceLastSignaledValue + 1;
		//g_pd3dCommandQueue->Signal(g_fence, fenceValue);
		//g_fenceLastSignaledValue = fenceValue;
		//frameCtx->FenceValue = fenceValue;
	}


	void RenderManager::Shutdown(re::RenderManager& renderManager)
	{
		#pragma message("TODO: Implement dx12::RenderManager::Shutdown")
		LOG_ERROR("TODO: Implement dx12::RenderManager::Shutdown");


		// TEMP DEBUG CODE:
		s_helloTriangle = nullptr;
	}
}
