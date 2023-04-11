// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "HashedDataObject.h"
#include "RootSignature_DX12.h"


namespace gr
{
	struct PipelineState;
}

namespace re
{
	class Shader;
	class TextureTargetSet;
}

namespace dx12
{
	class PipelineState final : public en::HashedDataObject
	{
	public:
		PipelineState();

		~PipelineState() { Destroy(); }

		void Destroy();

		void Create(gr::PipelineState const&, re::Shader const&, re::TextureTargetSet const&);
		
		ID3D12PipelineState* GetD3DPipelineState() const;

		dx12::RootSignature const& GetRootSignature() const;


	private:
		void ComputeDataHash() override;


	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

		dx12::RootSignature m_rootSignature;
	};
}