// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"

using Microsoft::WRL::ComPtr;
using glm::vec4;


// TEMP DEBUG CODE:
#include "MeshPrimitive_DX12.h"
#include "VertexStream_DX12.h"
#include "Debug_DX12.h"
#include "Config.h"
#include "Shader_DX12.h"
#include "PipelineState_DX12.h"
#include "TextureTarget_DX12.h"



// TEMP DEBUG CODE:
namespace
{
	using dx12::CheckHResult;

	static std::shared_ptr<re::MeshPrimitive> k_helloTriangle = nullptr;


	// TODO: Make this a platform function, and call it for all APIs during startup
	bool CreateAPIResources()
	{
		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		dx12::CommandQueue_DX12& copyQueue = ctxPlatParams->m_commandQueues[dx12::CommandQueue_DX12::Copy];

		ComPtr<ID3D12GraphicsCommandList2> commandList = copyQueue.GetCreateCommandList();

		
		dx12::MeshPrimitive::Create(*k_helloTriangle, commandList); // Internally creates all of the vertex stream resources

		std::shared_ptr<re::Shader> k_helloShader = std::make_shared<re::Shader>("HelloTriangle");
		dx12::Shader::Create(*k_helloShader);
		k_helloTriangle->GetMeshMaterial()->SetShader(k_helloShader);


		dx12::SwapChain::PlatformParams const* const swapChainParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(context.GetSwapChain().GetPlatformParams());


		// Create a pipeline state:
		// TODO: We shouldn't be using target sets for the backbuffer
		dx12::TextureTargetSet::PlatformParams* const swapChainTargetSetPlatParams =
			dynamic_cast<dx12::TextureTargetSet::PlatformParams*>(swapChainParams->m_backbufferTargetSets[0]->GetPlatformParams());

		gr::PipelineState defaultGrPipelineState{}; // Temp hax: Use a default gr::PipelineState
		std::shared_ptr<dx12::PipelineState> pso = dx12::Context::CreateAddPipelineState(
			defaultGrPipelineState,
			*k_helloShader, 
			swapChainTargetSetPlatParams->m_renderTargetFormats,
			swapChainTargetSetPlatParams->m_depthTargetFormat);


		// Execute command queue, and wait for it to be done (blocking)
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandLists[] =
		{
			commandList
		};

		uint64_t copyQueueFenceVal = copyQueue.Execute(1, commandLists);
		copyQueue.WaitForGPU(copyQueueFenceVal);



		return true;
	}


	void TransitionResource(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
		Microsoft::WRL::ComPtr<ID3D12Resource> resource,
		D3D12_RESOURCE_STATES stateBefore, 
		D3D12_RESOURCE_STATES stateAfter)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			resource.Get(),
			stateBefore,
			stateAfter);

		commandList->ResourceBarrier(1, &barrier);
	}


	// TODO: Should this be a member of a command list wrapper?
	void ClearRTV(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
		D3D12_CPU_DESCRIPTOR_HANDLE rtv, 
		glm::vec4 clearColor)
	{
		commandList->ClearRenderTargetView(
			rtv, 
			&clearColor.r, 
			0,			// Number of rectangles in the proceeding D3D12_RECT ptr
			nullptr);	// Ptr to an array of rectangles to clear in the resource view. Clears entire view if null
	}


	void ClearDepth(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
		D3D12_CPU_DESCRIPTOR_HANDLE dsv, 
		float depth)
	{
		commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
	}
}



namespace dx12
{
	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		#pragma message("TODO: Implement dx12::RenderManager::Initialize")
		LOG_ERROR("TODO: Implement dx12::RenderManager::Initialize");


		// TEMP DEBUG CODE:
		k_helloTriangle = meshfactory::CreateHelloTriangle();
		

		CreateAPIResources();
	}


	void RenderManager::Render(re::RenderManager& renderManager)
	{
		re::Context const& context = re::RenderManager::Get()->GetContext();

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		dx12::CommandQueue_DX12& directQueue = ctxPlatParams->m_commandQueues[dx12::CommandQueue_DX12::Direct];

		// Note: Our command lists and associated command allocators are already closed/reset
		ComPtr<ID3D12GraphicsCommandList2> commandList = directQueue.GetCreateCommandList();

		const uint8_t backbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());

		Microsoft::WRL::ComPtr<ID3D12Resource> backbufferResource =
			dx12::SwapChain::GetBackBufferResource(context.GetSwapChain());

		// Clear the render targets:
		// TODO: Move this to a helper "GetCurrentBackbufferRTVDescriptor" ?
		CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetView(
			ctxPlatParams->m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			backbufferIdx,
			ctxPlatParams->m_RTVDescSize);


		dx12::SwapChain::PlatformParams* const swapChainParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(context.GetSwapChain().GetPlatformParams());

		dx12::TextureTargetSet::PlatformParams* const depthTargetParams =
			dynamic_cast<dx12::TextureTargetSet::PlatformParams*>(swapChainParams->m_backbufferTargetSets[backbufferIdx]->GetPlatformParams());

		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = ctxPlatParams->m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

		// Clear the render targets.
		TransitionResource(
			commandList,
			backbufferResource,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		// Debug: Vary the clear color to easily verify things are working
		auto now = std::chrono::system_clock::now().time_since_epoch();
		size_t seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
		const float scale = static_cast<float>((glm::sin(seconds) + 1.0) / 2.0);

		const vec4 clearColor = vec4(0.38f, 0.36f, 0.1f, 1.0f) * scale;

		ClearRTV(commandList, renderTargetView, clearColor);
		ClearDepth(commandList, depthStencilView, 1.f);

		
		// Set the pipeline state:
		commandList->SetPipelineState(ctxPlatParams->m_pipelineState->GetD3DPipelineState());
		commandList->SetGraphicsRootSignature(ctxPlatParams->m_pipelineState->GetD3DRootSignature());

		// TEMP HAX: Get the position buffer/buffer view:
		dx12::VertexStream::PlatformParams_Vertex* const positionPlatformParams =
			dynamic_cast<dx12::VertexStream::PlatformParams_Vertex*>(k_helloTriangle->GetVertexStream(re::MeshPrimitive::Position)->GetPlatformParams());

		Microsoft::WRL::ComPtr<ID3D12Resource> positionBuffer = positionPlatformParams->m_bufferResource;
		D3D12_VERTEX_BUFFER_VIEW& positionBufferView = positionPlatformParams->m_vertexBufferView;

		// TEMP HAX: Get the color buffer/buffer view:
		dx12::VertexStream::PlatformParams_Vertex* const colorPlatformParams =
			dynamic_cast<dx12::VertexStream::PlatformParams_Vertex*>(k_helloTriangle->GetVertexStream(re::MeshPrimitive::Color)->GetPlatformParams());

		Microsoft::WRL::ComPtr<ID3D12Resource> colorBuffer = colorPlatformParams->m_bufferResource;
		D3D12_VERTEX_BUFFER_VIEW& colorBufferView = colorPlatformParams->m_vertexBufferView;

		// TEMP HAX: Get the index buffer/buffer view
		dx12::VertexStream::PlatformParams_Index* const indexPlatformParams =
			dynamic_cast<dx12::VertexStream::PlatformParams_Index*>(k_helloTriangle->GetVertexStream(re::MeshPrimitive::Indexes)->GetPlatformParams());

		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer = indexPlatformParams->m_bufferResource;
		D3D12_INDEX_BUFFER_VIEW& indexBufferView = indexPlatformParams->m_indexBufferView;


		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // TODO: This should be set by a batch, w.r.t MeshPrimitive::Drawmode

		commandList->IASetVertexBuffers(re::MeshPrimitive::Position, 1, &positionBufferView);
		commandList->IASetVertexBuffers(re::MeshPrimitive::Color, 1, &colorBufferView);

		commandList->IASetIndexBuffer(&indexBufferView);


	
		dx12::TextureTargetSet::SetViewport(*swapChainParams->m_backbufferTargetSets[backbufferIdx], commandList.Get());
		dx12::TextureTargetSet::SetScissorRect(*swapChainParams->m_backbufferTargetSets[backbufferIdx], commandList.Get());



		// Bind our render target(s) to the output merger (OM):
		commandList->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);

		//// Update the MVP matrix
		//XMMATRIX mvpMatrix = XMMatrixMultiply(m_ModelMatrix, m_ViewMatrix);
		//mvpMatrix = XMMatrixMultiply(mvpMatrix, m_ProjectionMatrix);
		//commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0);


		commandList->DrawIndexedInstanced(
			k_helloTriangle->GetVertexStream(re::MeshPrimitive::Indexes)->GetNumElements(),
			1,	// Instance count
			0,	// Start index location
			0,	// Base vertex location
			0);	// Start instance location


		// Transition our backbuffer resource back to the present state:
		// TODO: Move this to the present function?
		TransitionResource(
			commandList,
			backbufferResource,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandLists[] =
		{
			commandList
		};

		directQueue.Execute(1, commandLists);
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
		k_helloTriangle = nullptr;
	}
}
