// © 2022 Adam Badke. All rights reserved.
#include "Debug_DX12.h"
#include "RootSignature_DX12.h"
#include "Shader_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include <d3dcompiler.h> // We use this for the convenience of D3DReadFileToBlob

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void Shader::Create(re::Shader& shader)
	{
		dx12::Shader::PlatObj* platObj = shader.GetPlatformObject()->As<dx12::Shader::PlatObj*>();

		SEAssert(!platObj->m_isCreated, "Shader has already been created");
		platObj->m_isCreated = true;

		std::wstring const& shaderDirWStr = 
			core::Config::Get()->GetValueAsWString(core::configkeys::k_shaderDirectoryKey);

		constexpr wchar_t const* k_dx12ShaderExt = L".cso";

		SEAssert(!shader.m_metadata.empty(), "Shader does not contain any metadata");
		for (auto const& source : shader.m_metadata)
		{
			std::wstring const& filenameWStr = 
				shaderDirWStr + util::ToWideString(source.m_extensionlessFilename) + k_dx12ShaderExt;

			ComPtr<ID3DBlob> shaderBlob = nullptr;
			const HRESULT hr = ::D3DReadFileToBlob(filenameWStr.c_str(), &shaderBlob);
			CheckHResult(hr, "Failed to read shader file to blob");

			platObj->m_shaderBlobs[source.m_type] = shaderBlob;
		}

		// Now the shader blobs have been loaded, we can create the root signature:
		platObj->m_rootSignature = std::move(dx12::RootSignature::Create(shader));
	}


	void Shader::Destroy(re::Shader& shader)
	{
		dx12::Shader::PlatObj* platObj = shader.GetPlatformObject()->As<dx12::Shader::PlatObj*>();

		if (!platObj->m_isCreated)
		{
			return;
		}

		std::fill(platObj->m_shaderBlobs.begin(), platObj->m_shaderBlobs.end(), nullptr);
		platObj->m_isCreated = false;
	}


	dx12::RootSignature* Shader::GetRootSignature(re::Shader const& shader)
	{
		dx12::Shader::PlatObj* platObj = shader.GetPlatformObject()->As<dx12::Shader::PlatObj*>();
		SEAssert(platObj->m_isCreated, "Shader has not been created");

		return platObj->m_rootSignature.get();
	}
}