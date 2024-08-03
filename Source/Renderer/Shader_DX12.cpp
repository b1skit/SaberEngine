// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Core/Config.h"
#include "Debug_DX12.h"
#include "RootSignature_DX12.h"
#include "Shader_DX12.h"

#include <d3dcompiler.h> // We use this for the convenience of D3DReadFileToBlob

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void Shader::Create(re::Shader& shader)
	{
		dx12::Shader::PlatformParams* platformParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		SEAssert(!platformParams->m_isCreated, "Shader has already been created");
		platformParams->m_isCreated = true;

		std::wstring const& shaderDirWStr = 
			core::Config::Get()->GetValueAsWString(core::configkeys::k_shaderDirectoryKey);

		constexpr wchar_t const* k_dx12ShaderExt = L".cso";

		for (auto const& source : shader.m_extensionlessSourceFilenames)
		{
			std::wstring const& filenameWStr = shaderDirWStr + util::ToWideString(source.first) + k_dx12ShaderExt;

			ComPtr<ID3DBlob> shaderBlob = nullptr;
			const HRESULT hr = ::D3DReadFileToBlob(filenameWStr.c_str(), &shaderBlob);
			CheckHResult(hr, "Failed to read shader file to blob");

			const re::Shader::ShaderType shaderType = source.second;
			platformParams->m_shaderBlobs[shaderType] = shaderBlob;
		}

		// Now the shader blobs have been loaded, we can create the root signature:
		platformParams->m_rootSignature = std::move(dx12::RootSignature::Create(shader));
	}


	void Shader::Destroy(re::Shader& shader)
	{
		dx12::Shader::PlatformParams* params = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		if (!params->m_isCreated)
		{
			return;
		}

		params->m_shaderBlobs = { 0 };
		params->m_isCreated = false;
	}


	dx12::RootSignature* Shader::GetRootSignature(re::Shader const& shader)
	{
		dx12::Shader::PlatformParams* platformParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();
		SEAssert(platformParams->m_isCreated, "Shader has not been created");

		return platformParams->m_rootSignature.get();
	}
}