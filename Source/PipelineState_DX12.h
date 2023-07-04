// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "HashedDataObject.h"
#include "RootSignature_DX12.h"


namespace gr
{
	class PipelineState;
}

namespace re
{
	class Shader;
	class TextureTargetSet;
}

namespace dx12
{
	class PipelineState
	{
	public:
		PipelineState();

		~PipelineState() { Destroy(); }

		void Destroy();

		void Create(re::Shader const&, gr::PipelineState const&, re::TextureTargetSet const&);
		
		ID3D12PipelineState* GetD3DPipelineState() const;

		dx12::RootSignature const* GetRootSignature() const;


	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

		dx12::RootSignature* m_rootSignature;
	};
}