// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RootSignature_DX12.h"
#include "Shader.h"


namespace dx12
{
	class Shader
	{
	public:
		struct PlatObj final : public re::Shader::PlatObj
		{
			std::array<Microsoft::WRL::ComPtr<ID3DBlob>, re::Shader::ShaderType_Count> m_shaderBlobs = {0};
			
			std::unique_ptr<dx12::RootSignature> m_rootSignature;
		};


	public:
		static void Create(re::Shader& shader);
		static void Destroy(re::Shader& shader);
		[[nodiscard]] static dx12::RootSignature* GetRootSignature(re::Shader const& shader);
	};
}