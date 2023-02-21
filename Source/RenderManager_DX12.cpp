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
#include "MeshPrimitive.h"
#include "MeshPrimitive_DX12.h"
#include "VertexStream_DX12.h"
#include "Debug_DX12.h"
#include "Config.h"
#include "Shader_DX12.h"



// TEMP DEBUG CODE:
namespace
{
	using Microsoft::WRL::ComPtr;
	using dx12::CheckHResult;

	static std::shared_ptr<re::MeshPrimitive> k_helloTriangle = nullptr;

	// Depth buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBufferResource;

	// Descriptor heap for depth buffer
	static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap; // ComPtr to an array of DSV descriptors

	// Pipeline state object: Can create an unlimited number (typically at initialization time), & select at runtime
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

	// TODO: Move this to TextureTarget
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect; // Note: Geometry shaders use multiple scissor rects


	void UpdateBufferResource(
		ComPtr<ID3D12GraphicsCommandList2> commandList,
		ID3D12Resource** pDestinationResource,
		ID3D12Resource** pIntermediateResource,
		size_t numElements, 
		size_t elementSize, 
		const void* bufferData,
		D3D12_RESOURCE_FLAGS flags)
	{
		SEAssert("Buffer data cannot be null", bufferData);

		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		Microsoft::WRL::ComPtr<ID3D12Device2> device = ctxPlatParams->m_device.GetDisplayDevice();

		size_t bufferSize = numElements * elementSize;

		const CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

		// Create a committed resource for the GPU resource in a default heap
		HRESULT hr = device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(pDestinationResource));

		// Create an committed resource for the upload
		const CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		const CD3DX12_RESOURCE_DESC committedresourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

		hr = ctxPlatParams->m_device.GetDisplayDevice()->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&committedresourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIntermediateResource));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		::UpdateSubresources(
			commandList.Get(),
			*pDestinationResource,
			*pIntermediateResource,
			0,						// Index of 1st subresource in the resource
			0,						// Number of subresources in the resource.
			1,						// Required byte size for the update
			&subresourceData);
	}


	void CreateDepthBuffer(int width, int height)
	{
		// NOTE: We assume the depth buffer is not in use. If this ever changes (eg. we're recreating the depth buffer,
		// and it may still be in flight, we should ensure we flush all commands that might reference it first

		SEAssert("Invalid dimensions", width >= 1 && height >= 1);

		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		ComPtr<ID3D12Device2> device = ctxPlatParams->m_device.GetDisplayDevice();

		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		CD3DX12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			width,
			height,
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		HRESULT hr = device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&m_depthBufferResource)
		);

		// Update the depth-stencil view
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(
			m_depthBufferResource.Get(),
			&dsv,
			m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
	}


	bool LoadResources()
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


		// Create the descriptor heap for the depth-stencil view.
		ComPtr<ID3D12Device2> device = ctxPlatParams->m_device.GetDisplayDevice();

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		HRESULT hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap));
		CheckHResult(hr, "Failed to create descriptor heap");


		// Create a root signature
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		// A single 32-bit constant root parameter that is used by the vertex shader
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsConstants(
			sizeof(glm::mat4) / 4, 
			0, 
			0, 
			D3D12_SHADER_VISIBILITY_VERTEX);

		// Create the root signature description:
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		// Serialize the root signature.
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		hr = D3DX12SerializeVersionedRootSignature(
			&rootSignatureDescription,
			featureData.HighestVersion, 
			&rootSignatureBlob, 
			&errorBlob);
		CheckHResult(hr, "Failed to serialize versioned root signature");

		// Create the root signature.
		hr = device->CreateRootSignature(
			0, 
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(), 
			IID_PPV_ARGS(&ctxPlatParams->m_rootSignature));
		CheckHResult(hr, "Failed to create root signature");


		// Create the vertex input layout
		// TODO: This should be created by a member of the MeshPrimitive_DX12
		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{
				"POSITION",									// Semantic name
				0,											// Semantic idx: Only needed when >1 element of same semantic
				DXGI_FORMAT_R32G32B32_FLOAT,				// Format
				re::MeshPrimitive::Position,				// Input slot [0, 15]
				D3D12_APPEND_ALIGNED_ELEMENT,				// Aligned byte offset
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,	// Input slot class
				0											// Input data step rate
			},
			{
				"COLOR",
				0,
				DXGI_FORMAT_R32G32B32A32_FLOAT,
				re::MeshPrimitive::Color,
				D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			},
		};

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;


		dx12::Shader::PlatformParams* const shaderParams =
			dynamic_cast<dx12::Shader::PlatformParams* const>(k_helloShader->GetPlatformParams());

		// TODO: Move this to the PipelineState object
		// Stage PSO? Target?
		// -> Build from stage pipeline state + Shaders, etc
		//		-> Once, after pipeline is created
		// TODO: Set up the depth bias correctly for shadows
		D3D12_RASTERIZER_DESC rasterizerDesc = D3D12_RASTERIZER_DESC();
		rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = true;
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.DepthBiasClamp = 0.f;
		rasterizerDesc.SlopeScaledDepthBias = 0.f;
		rasterizerDesc.DepthClipEnable = true;
		rasterizerDesc.MultisampleEnable = false;
		rasterizerDesc.AntialiasedLineEnable = false; // Only applies if drawing lines with .MultisampleEnable = false
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE::D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		// TODO: Should this be defined in a common space?
		struct PipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS vShader;
			CD3DX12_PIPELINE_STATE_STREAM_PS pShader;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = ctxPlatParams->m_rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, _countof(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[dx12::Shader::Vertex].Get());
		pipelineStateStream.pShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[dx12::Shader::Pixel].Get());

		pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pipelineStateStream.RTVFormats = rtvFormats;
		pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(rasterizerDesc);
		

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream),
			&pipelineStateStream
		};
		hr = device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
		CheckHResult(hr, "Failed to create pipeline state");


		// Execute command queue, and wait for it to be done (blocking)
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandLists[] =
		{
			commandList
		};

		uint64_t copyQueueFenceVal = copyQueue.Execute(1, commandLists);
		copyQueue.WaitForGPU(copyQueueFenceVal);


		// Create the depth buffer		
		CreateDepthBuffer(
			en::Config::Get()->GetValue<int>("windowXRes"), 
			en::Config::Get()->GetValue<int>("windowYRes"));


		// Compositor setup:
		m_viewport = CD3DX12_VIEWPORT(
			0.0f,
			0.0f,
			static_cast<float>(en::Config::Get()->GetValue<int>("windowXRes")),
			static_cast<float>(en::Config::Get()->GetValue<int>("windowYRes")));

		m_scissorRect = CD3DX12_RECT(
			0, 
			0, 
			LONG_MAX, 
			LONG_MAX);

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
		

		LoadResources();
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

		// TODO: Move the dsv stuff to the context, for now...
		auto depthStencilView = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

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
		commandList->SetPipelineState(m_pipelineState.Get());
		commandList->SetGraphicsRootSignature(ctxPlatParams->m_rootSignature.Get());


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

		commandList->RSSetViewports(1, &m_viewport);
		commandList->RSSetScissorRects(1, &m_scissorRect);

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
	}
}
