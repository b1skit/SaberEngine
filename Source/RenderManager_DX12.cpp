// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <wrl.h>

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "Context_DX12.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;
	using glm::vec4;


	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		#pragma message("TODO: Implement dx12::RenderManager::Initialize")
		LOG_ERROR("TODO: Implement dx12::RenderManager::Initialize");
	}


	void RenderManager::Render(re::RenderManager& renderManager)
	{
		// TODO: Replace all of these direct accesss via the platform params with dx12-layer getters/setters

		re::Context const& context = renderManager.GetContext();

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		dx12::SwapChain::PlatformParams* const swapChainPlatParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(context.GetSwapChain().GetPlatformParams());
		
		const uint8_t backbufferIdx = swapChainPlatParams->m_backBufferIdx;


		// Reset our command allocator and command list to their original states, so we can start recording commands
		// Note: Our command lists are closed immediately after they were created
		ctxPlatParams->m_commandLists[backbufferIdx].Reset(nullptr);

		// Clear the render target:
		ComPtr<ID3D12Resource>& backBuffer = swapChainPlatParams->m_backBuffers[backbufferIdx];

		// First, transition our resource (back) to a render target state:
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(), // Resource ptr
			D3D12_RESOURCE_STATE_PRESENT, // State before
			D3D12_RESOURCE_STATE_RENDER_TARGET); // State after

		// Record the transition on the command list:
		ctxPlatParams->m_commandLists[backbufferIdx].AddResourceBarrier(1, &barrier);

		// Construct a CPU descriptor handle to a render target view.
		// Our RTV is offset from the beginning of the descriptor heap using an index and descriptor size
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
			ctxPlatParams->m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			backbufferIdx,
			ctxPlatParams->m_RTVDescSize);

		// Record our clear RTV command:
		const vec4 clearColor = vec4(0.38f, 0.36f, 0.1f, 1.0f);
		ctxPlatParams->m_commandLists[backbufferIdx].ClearRTV(
			rtv, // Descriptor we're clearning
			clearColor,
			0, // Number of rectangles in the proceeding D3D12_RECT ptr
			nullptr); // Ptr to an array of rectangles to clear in the resource view. Clears the entire view if null
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
	}
}
