// © 2022 Adam Badke. All rights reserved.
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "CoreEngine.h"
#include "Debug_DX12.h"
#include "DebugConfiguration.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"
#include "Window_Win32.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


	Context::PlatformParams::PlatformParams()
		: m_RTVDescHeap(nullptr)
		, m_RTVDescSize(0)
	{
		for (uint32_t i = 0; i < dx12::RenderManager::k_numFrames; i++)
		{
			m_frameFenceValues[i] = 0;
		}
	}


	void Context::Create(re::Context& context)
	{
		dx12::Context::PlatformParams* ctxPlatParams =
			context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		EnableDebugLayer();

		ctxPlatParams->m_device.Create();

		// TODO: Create/support more command queue types

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		ctxPlatParams->m_commandQueues[CommandQueue::CommandListType::Direct].Create(
			device, CommandQueue::CommandListType::Direct);

		ctxPlatParams->m_commandQueues[CommandQueue::CommandListType::Copy].Create(
			device, CommandQueue::CommandListType::Copy);


		// RTV Descriptor Heap:
		ctxPlatParams->m_RTVDescHeap = CreateDescriptorHeap(
			device,
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			k_numRTVDescriptors);
		ctxPlatParams->m_RTVDescSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		
		LOG("RTV descriptor size is %dB", ctxPlatParams->m_RTVDescSize);

		// DSV Descriptor Heap:
		ctxPlatParams->m_DSVHeap = CreateDescriptorHeap(
			ctxPlatParams->m_device.GetD3DDisplayDevice(),
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			k_numDSVDescriptors);
		ctxPlatParams->m_DSVDescSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		LOG("DSV descriptor size is %dB", ctxPlatParams->m_DSVDescSize);

		// NOTE: Currently, this call retrieves m_commandQueue from the Context platform params
		// TODO: Clean this up, it's gross.
		context.GetSwapChain().Create();
		
		dx12::SwapChain::PlatformParams* swapChainParams = 
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		SEAssert("Window pointer cannot be null", en::CoreEngine::Get()->GetWindow());
		win32::Window::PlatformParams* windowPlatParams = 
			en::CoreEngine::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		dx12::TextureTargetSet::PlatformParams* swapChainTargetSetParams = 
			swapChainParams->m_backbufferTargetSets[0]->GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		// Setup our ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = re::k_imguiIniPath;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);
		ImGui_ImplDX12_Init(
			ctxPlatParams->m_device.GetD3DDisplayDevice(),
			dx12::RenderManager::k_numFrames, // Number of frames in flight
			swapChainTargetSetParams->m_renderTargetFormats.RTFormats[0],
			ctxPlatParams->m_RTVDescHeap.Get(),
			ctxPlatParams->m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			ctxPlatParams->m_RTVDescHeap->GetGPUDescriptorHandleForHeapStart());
	}


	void Context::Destroy(re::Context& context)
	{
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		if (!ctxPlatParams)
		{
			return;
		}

		// ImGui Cleanup:
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();		

		// Make sure our command queues have finished all commands before closing.
		ctxPlatParams->m_commandQueues[CommandQueue::Copy].Flush();
		ctxPlatParams->m_commandQueues[CommandQueue::Copy].Destroy();
		
		ctxPlatParams->m_commandQueues[CommandQueue::Direct].Flush();
		ctxPlatParams->m_commandQueues[CommandQueue::Direct].Destroy();

		ctxPlatParams->m_RTVDescHeap = nullptr;
		ctxPlatParams->m_DSVHeap = nullptr;

		ctxPlatParams->m_device.Destroy();
	}


	void Context::Present(re::Context const& context)
	{
		dx12::SwapChain::PlatformParams* swapChainPlatParams = 
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		const uint8_t backbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());

		// Present the backbuffer:
		const bool vsyncEnabled = swapChainPlatParams->m_vsyncEnabled;
		const uint32_t syncInterval = vsyncEnabled ? 1 : 0;
		const uint32_t presentFlags = 
			(swapChainPlatParams->m_tearingSupported && !vsyncEnabled) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		swapChainPlatParams->m_swapChain->Present(syncInterval, presentFlags);

		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		// Insert a signal into the command queue:
		ctxPlatParams->m_frameFenceValues[backbufferIdx] = 
			ctxPlatParams->m_commandQueues[CommandQueue::Direct].Signal();
		// TODO: We should maintain a frame fence, and individual fences per command queue

		// Get the next backbuffer index:
		// Note: Backbuffer indices are not guaranteed to be sequential if we're using DXGI_SWAP_EFFECT_FLIP_DISCARD
		swapChainPlatParams->m_backBufferIdx = swapChainPlatParams->m_swapChain->GetCurrentBackBufferIndex();
		
		// Wait on the fence for the next backbuffer, to ensure its previous frame is done (blocking)
		ctxPlatParams->m_commandQueues[CommandQueue::Direct].WaitForGPU(ctxPlatParams->m_frameFenceValues[backbufferIdx]);
	}


	std::shared_ptr<dx12::PipelineState> Context::CreateAddPipelineState(
		gr::PipelineState const& grPipelineState, 
		re::Shader const& shader, 
		D3D12_RT_FORMAT_ARRAY const& rtvFormats, 
		const DXGI_FORMAT dsvFormat)
	{
		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		// TEMP HAX: For now, we just have a single PSO, so just hard-code it. TODO: Create a library of pre-computed
		// PSOs at startup
		ctxPlatParams->m_pipelineState = std::make_shared<dx12::PipelineState>(
			grPipelineState,
			&shader,
			rtvFormats, 
			dsvFormat);

		LOG_ERROR("TODO: Implement dx12::Context::CreateAddPipelineState correctly");

		return ctxPlatParams->m_pipelineState;
	}


	uint8_t Context::GetMaxTextureInputs()
	{
		SEAssertF("TODO: Implement this");
		return 0;
	}


	uint8_t Context::GetMaxColorTargets()
	{
		return D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}


	CommandQueue& GetCommandQueue(CommandQueue::CommandListType type)
	{
		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		return ctxPlatParams->m_commandQueues[type];
	}


	ComPtr<ID3D12DescriptorHeap> Context::CreateDescriptorHeap(
		ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
	{
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		ComPtr<ID3D12DescriptorHeap> descriptorHeap;

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type; // What's in our heap? CBV/SRV/UAV, sampler, RTV, DSV
		desc.NumDescriptors = numDescriptors;
		//desc.Flags = ; // TODO: Do we need any specific flags?
		desc.NodeMask = deviceNodeMask;

		HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
		CheckHResult(hr, "Failed to create descriptor heap");

		return descriptorHeap;
	}
}