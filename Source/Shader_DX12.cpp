// © 2022 Adam Badke. All rights reserved.
#include "Core\Assert.h"
#include "Config.h"
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

		// Our DX12 Shaders have a naming pattern of <name>_<V/G/P/C>Shader.hlsl
		// e.g. Some_VShader.hlsl, Some_GShader.hlsl, Some_PShader.hlsl, Some_CShader.hlsl
		// Compiled Shader Objects (CSO) are pre-compiled by Visual Studio, we attempt to load them here
		const std::wstring shaderBaseName = std::wstring(shader.GetName().begin(), shader.GetName().end());

		constexpr std::array<wchar_t const*, ShaderType_Count> nameSuffix =
		{
			L"_VShader.cso",
			L"_GShader.cso",
			L"_PShader.cso",
			L"_HShader.cso",
			L"_DShader.cso",
			L"_MShader.cso",
			L"_AShader.cso",
			L"_CShader.cso"
		};

		// Assemble root shader dir, as a wide string
		std::wstring const& shaderRootWStr = 
			en::Config::Get()->GetValueAsWString(core::configkeys::k_shaderDirectoryKey) + shaderBaseName;

		// Load the shader blobs:
		for (size_t i = 0; i < nameSuffix.size(); i++)
		{
			const std::wstring shaderName = shaderRootWStr + nameSuffix[i];

			ComPtr<ID3DBlob> shaderBlob = nullptr;
			HRESULT hr = ::D3DReadFileToBlob(shaderName.c_str(), &shaderBlob);

			if (SUCCEEDED(hr))
			{
				platformParams->m_shaderBlobs[i] = shaderBlob;
			}
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