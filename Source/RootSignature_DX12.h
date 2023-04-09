// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "HashedDataObject.h"


namespace dx12
{
	class RootSignature final : public en::HashedDataObject
	{
	public:
		static constexpr uint32_t k_totalRootSigDescriptorTableIndices = 32;


	public:
		RootSignature();

		~RootSignature();
		void Destroy();

		uint32_t GetDescriptorTableIdxBitmask() const;
		uint32_t GetNumDescriptors(uint8_t rootIndex) const;

		ID3D12RootSignature* GetD3DRootSignature() const;

		D3D12_ROOT_SIGNATURE_DESC1 const& GetD3DRootSignatureDesc() const;


	private:
		void ComputeDataHash() override;


	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
		D3D12_ROOT_SIGNATURE_DESC1 m_rootSigDescription;

		uint32_t m_descriptorTableIdxBitmask; 
		uint32_t m_numDescriptorsPerTable[k_totalRootSigDescriptorTableIndices];
	};
}