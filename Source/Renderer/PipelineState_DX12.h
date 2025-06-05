// © 2022 Adam Badke. All rights reserved.
#pragma once


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

		void Create(re::Shader const&, re::TextureTargetSet const*);
		
		ID3D12PipelineState* GetD3DPipelineState() const;


	private:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	};
}