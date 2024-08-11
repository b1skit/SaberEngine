// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include <wrl.h>
#include <d3d12.h>


namespace dx12
{
	class RootSignature;
	

	class Shader
	{
	public:
		struct PlatformParams final : public re::Shader::PlatformParams
		{
			std::array<Microsoft::WRL::ComPtr<ID3DBlob>, re::Shader::ShaderType_Count> m_shaderBlobs = {0};
			
			std::unique_ptr<dx12::RootSignature> m_rootSignature;
		};

		// TODO: Handle copying of d3dcompiler_47.dll into the same folder as the compiled Saber Engine .exe 

	public:
		static void Create(re::Shader& shader);
		static void Destroy(re::Shader& shader);
		[[nodiscard]] static dx12::RootSignature* GetRootSignature(re::Shader const& shader);
	};
}