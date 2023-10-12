// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "HashedDataObject.h"
#include "RootSignature_DX12.h"


namespace re
{
	class PipelineState;
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

		void Create(re::Shader const&, re::PipelineState const&, re::TextureTargetSet const&);
		
		ID3D12PipelineState* GetD3DPipelineState() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	};
}