// © 2022 Adam Badke. All rights reserved.
#include <d3dcompiler.h>

#include "Config.h"
#include "DebugConfiguration.h"
#include "Shader_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void Shader::Create(re::Shader& shader)
	{
		dx12::Shader::PlatformParams* params = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		SEAssert("Shader has already been created", !params->m_isCreated);
		params->m_isCreated = true;

		// Our DX12 Shaders have a naming pattern of <name>_<V/G/P/C>Shader.hlsl
		// e.g. Some_VShader.hlsl, Some_GShader.hlsl, Some_PShader.hlsl, Some_CShader.hlsl
		// Compiled Shader Objects (CSO) are pre-compiled by Visual Studio, we attempt to load them here
		const std::wstring shaderBaseName = std::wstring(shader.GetName().begin(), shader.GetName().end());

		constexpr std::array<wchar_t const*, ShaderType_Count> nameSuffix =
		{
			L"_VShader.cso",
			L"_GShader.cso",
			L"_PShader.cso",
			L"_CShader.cso"
		};

		// Assemble root shader dir, as a wide string
		std::wstring const& shaderRootWStr = en::Config::Get()->GetValueAsWString("shaderDirectory");

		for (size_t i = 0; i < nameSuffix.size(); i++)
		{
			const std::wstring shaderName = shaderRootWStr + shaderBaseName + nameSuffix[i];

			ComPtr<ID3DBlob> shaderBlob = nullptr;
			HRESULT hr = D3DReadFileToBlob(shaderName.c_str(), &shaderBlob);

			if (SUCCEEDED(hr))
			{
				params->m_shaderBlobs[i] = shaderBlob;
			}
		}

		params->m_isCreated = true;
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
}