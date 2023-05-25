// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	DXGI_FORMAT Texture::GetTextureFormat(re::Texture::TextureParams const& texParams)
	{	
		switch (texParams.m_format)
		{
			case re::Texture::Format::RGBA32F: // 32 bits per channel x N channels
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			break;
			case re::Texture::Format::RG32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;
			}
			break;
			case re::Texture::Format::R32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
			}
			break;
			case re::Texture::Format::RGBA16F: // 16 bits per channel x N channels
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
			}
			break;
			case re::Texture::Format::RG16F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT;
			}
			break;
			case re::Texture::Format::R16F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;
			}
			break;
			case re::Texture::Format::RGBA8: // 8 bits per channel x N channels
			{
				return texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ?
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			break;
			case re::Texture::Format::RG8:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM;
			}
			break;
			case re::Texture::Format::R8:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R8_UNORM;
			}
			break;
			case re::Texture::Format::Depth32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT;
			}
			break;
			case re::Texture::Format::Invalid:
			default:
			{
				SEAssertF("Invalid format");
			}
		}
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	}


	Texture::PlatformParams::PlatformParams(re::Texture::TextureParams const& texParams)
	{
		m_format = GetTextureFormat(texParams);
	}


	Texture::PlatformParams::~PlatformParams()
	{
		m_format = DXGI_FORMAT_UNKNOWN;
		m_textureResource = nullptr;
		m_cpuDescAllocation.Free(0);
	}


	void Texture::Create(
		re::Texture& texture,
		ComPtr<ID3D12GraphicsCommandList2> commandList, 
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture is already created", texPlatParams->m_isCreated == false);
		texPlatParams->m_isCreated = true;

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		// D3D12 Initial resource states:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#initial-states-for-resources
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

		uint32_t numSubresources = 0;

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();
		switch (texParams.m_usage)
		{
		case re::Texture::Usage::Color:
		{
			//initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST; // We need to buffer the texture data
			//numSubresources = 1; // TODO: Support mips

			//re::Texture::TextureParams const& texParams = texture.GetTextureParams();	
			//
			//D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_NONE;

			//// Resource description:
			//CD3DX12_RESOURCE_DESC colorResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			//	texPlatParams->m_format,
			//	texParams.m_width,
			//	texParams.m_height,
			//	1,			// arraySize
			//	0,			// mipLevels. TODO: Support mips
			//	1,			// sampleCount TODO: Support MSAA
			//	0,			// sampleQuality
			//	flags);

			//CD3DX12_HEAP_PROPERTIES colorHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			//HRESULT hr = device->CreateCommittedResource(
			//	&colorHeapProperties,
			//	D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
			//	&colorResourceDesc,
			//	initialState,
			//	nullptr, // Optimized clear value: Must be NULL except for buffers, render, or depth-stencil targets
			//	IID_PPV_ARGS(&texPlatParams->m_textureResource));
			//texPlatParams->m_textureResource->SetName(texture.GetWName().c_str());



			//// Create an intermediate upload heap:

			//const size_t numBytes =
			//	texParams.m_faces * texParams.m_width * texParams.m_height * re::Texture::GetNumBytesPerTexel(texParams.m_format);
			//SEAssert("Color target must have data to buffer", texture.GetTexels().size() == numBytes);

			//

			//const CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			//const CD3DX12_RESOURCE_DESC committedresourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			//	texPlatParams->m_format,
			//	texParams.m_width,
			//	texParams.m_height,
			//	texParams.m_faces,
			//	0, // TODO: Support mips
			//	1, // Sample count. TODO: Support MSAA
			//	0, // Sample quality
			//	flags);

			//ComPtr<ID3D12Resource> itermediateBufferResource = nullptr;

			//hr = device->CreateCommittedResource(
			//	&uploadHeapProperties,
			//	D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
			//	&committedresourceDesc,
			//	D3D12_RESOURCE_STATE_GENERIC_READ,
			//	nullptr,
			//	IID_PPV_ARGS(&itermediateBufferResource));
			//CheckHResult(hr, "Failed to create intermediate texture buffer resource");

			//itermediateBufferResource->SetName(L"Color texture intermediate buffer");

			//// Populate the subresource:
			//D3D12_SUBRESOURCE_DATA subresourceData = {};
			//subresourceData.pData = &texture.GetTexels()[0];
			//subresourceData.RowPitch = numBytes; // ??????????????????????????????????????????????
			//subresourceData.SlicePitch = numBytes; // ????????????????????????????????????????????

			//::UpdateSubresources(
			//	commandList.Get(),
			//	texPlatParams->m_textureResource.Get(),		// Destination resource
			//	itermediateBufferResource.Get(),			// Intermediate resource
			//	0,								// Index of 1st subresource in the resource
			//	0,								// Number of subresources in the resource.
			//	1,								// Required byte size for the update
			//	&subresourceData);




			//// Released once the copy is done
			//intermediateResources.emplace_back(itermediateBufferResource);
		}
		break;
		case re::Texture::Usage::ColorTarget:
		{
			// TODO: ColorTargets are textures too
			LOG_ERROR("dx12::Texture::Create(): Texture is marked as a target, doing nothing...");
		}
		break;
		case re::Texture::Usage::SwapchainColorProxy:
		{
			initialState = D3D12_RESOURCE_STATE_COMMON;
			numSubresources = 1;
		}
		break;
		case re::Texture::Usage::DepthTarget:
		{
			initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE;
			numSubresources = 1;

			re::Texture::TextureParams const& texParams = texture.GetTextureParams();

			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			// Depth resource description:
			CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				texPlatParams->m_format,
				texParams.m_width,
				texParams.m_height,
				1,			// arraySize
				0,			// mipLevels
				1,			// sampleCount
				0,			// sampleQuality
				flags);

			// Depth committed heap resources:
			CD3DX12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			// Clear values:
			D3D12_CLEAR_VALUE optimizedClearValue = {};
			optimizedClearValue.Format = texPlatParams->m_format;
			optimizedClearValue.DepthStencil = { 1.0f, 0 }; // Float depth, uint8_t stencil

			HRESULT hr = device->CreateCommittedResource(
				&depthHeapProperties,
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
				&depthResourceDesc,
				initialState,
				&optimizedClearValue,
				IID_PPV_ARGS(&texPlatParams->m_textureResource));
			texPlatParams->m_textureResource->SetName(texture.GetWName().c_str());

			// TODO: Combine common code outside of this switch!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		}
		break;
		case re::Texture::Usage::Invalid:
		default:
		{
			SEAssertF("Invalid texture usage");
		}
		}

		texPlatParams->m_isDirty = true;

		// Register the resource with the global resource state tracker:
		dx12::Context::GetGlobalResourceStateTracker().RegisterResource(
			texPlatParams->m_textureResource.Get(),
			initialState,
			numSubresources);
	}


	// re::Texture::Create factory wrapper, for the DX12-specific case where we need to create a Texture resource using
	// an existing ID3D12Resource
	std::shared_ptr<re::Texture> Texture::CreateFromExistingResource(
		std::string const& name, 
		re::Texture::TextureParams const& params, 
		bool doClear, 
		Microsoft::WRL::ComPtr<ID3D12Resource> textureResource)
	{
		SEAssert("Invalid/unexpected texture format. For now, this function is used to create a backbuffer color target",
			params.m_usage == re::Texture::Usage::SwapchainColorProxy);

		// Note: re::Texture::Create will enroll the texture in API object creation, and eventually call the standard 
		// dx12::Texture::Create above
		std::shared_ptr<re::Texture> newTexture = re::Texture::Create(name, params, doClear);

		dx12::Texture::PlatformParams* texPlatParams = 
			newTexture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture is already created", 
			!texPlatParams->m_isCreated && texPlatParams->m_textureResource == nullptr);

		texPlatParams->m_textureResource = textureResource;

		return newTexture;
	}


	void Texture::Destroy(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// Unregister the resource from the global resource state tracker. Note: The resource might be null if it was
		// never created (e.g. a duplicate was detected after loading)
		if (texPlatParams->m_textureResource)
		{
			dx12::Context::GetGlobalResourceStateTracker().UnregisterResource(texPlatParams->m_textureResource.Get());
		}

		texPlatParams->m_textureResource = nullptr;
		texPlatParams->m_cpuDescAllocation.Free(0);

		texPlatParams->m_isCreated = false;
		texPlatParams->m_isDirty = true;
	}
	
	
	void Texture::GenerateMipMaps(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::Texture::GenerateMipMaps")
		SEAssertF("TODO: Implement this");
	}
}