// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>


namespace gr
{
	struct PipelineState;
}

namespace re
{
	class Shader;
}

namespace dx12
{
	// TODO: Make this a hashed object, so we can find/reuse/batch PSOs

	class PipelineState
	{
	public:
		PipelineState(
			gr::PipelineState const&, re::Shader const*, D3D12_RT_FORMAT_ARRAY const& rtvFormats, const DXGI_FORMAT dsvFormat);

		~PipelineState() { Destroy(); }

		void Destroy();
		
		ID3D12PipelineState* GetD3DPipelineState() const;
		ID3D12RootSignature* GetD3DRootSignature() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

	private:
		PipelineState() = delete;
	};
}