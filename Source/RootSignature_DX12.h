// � 2023 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "HashedDataObject.h"
#include "Shader_DX12.h"


namespace re
{
	class Shader;
}

namespace dx12
{
	class RootSignature final
	{
	public:
		static constexpr uint32_t k_totalRootSigDescriptorTableIndices = 32;

		static constexpr uint8_t k_invalidRootSigIndex = std::numeric_limits<uint8_t>::max();
		static constexpr uint8_t k_invalidRegisterVal = std::numeric_limits<uint8_t>::max();
		static constexpr uint32_t k_invalidOffset = std::numeric_limits<uint32_t>::max();


	public: // Shader reflection metadata		
		enum class EntryType
		{
			RootConstant,
			RootCBV,
			RootSRV,
			// TODO: More Root__ types

			TextureSRV, // Packed into descriptor tables

			Sampler,

			EntryType_Count,
			EntryType_Invalid = EntryType_Count
		};
		struct RootSigEntry
		{
			EntryType m_type		= EntryType::EntryType_Invalid;
			uint8_t m_rootSigIndex	= k_invalidRootSigIndex;

			uint8_t m_baseRegister	= k_invalidRegisterVal;
			uint8_t m_registerSpace = k_invalidRegisterVal;

			uint32_t m_offset		= k_invalidOffset; // Descriptor tables only: Offset into table
			uint32_t m_count		= 0; // Root constants/Descriptor tables: No. of 32-bit values/No. of descriptors

			D3D12_SHADER_VISIBILITY m_shaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;

			bool IsValid() const
			{ 
				return m_type != EntryType::EntryType_Invalid &&
					m_rootSigIndex != k_invalidRootSigIndex &&
					m_baseRegister != k_invalidRegisterVal &&
					m_registerSpace != k_invalidRegisterVal &&
					m_offset != k_invalidOffset &&
					m_count != 0;
			};
		};


	public:
		RootSignature();
		~RootSignature();
		void Destroy();

		// TODO: This should be an object factory, that returns a (possibly pre-existing) root signature object
		void Create(re::Shader const&);

		uint32_t GetDescriptorTableIdxBitmask() const;
		uint32_t GetNumDescriptorsInTable(uint8_t rootIndex) const;

		ID3D12RootSignature* GetD3DRootSignature() const;

		D3D12_ROOT_SIGNATURE_DESC1 const& GetD3DRootSignatureDesc() const;

		RootSigEntry const* GetRootSignatureEntry(std::string const& resourceName) const;
		bool HasResource(std::string const& resourceName) const;

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC m_rootSigDescription;

	
	private: // Track which root sig indexes contain descriptor tables, and how many entries they have
		uint32_t m_rootSigDescriptorTableIdxBitmask; 
		uint32_t m_numDescriptorsPerTable[k_totalRootSigDescriptorTableIndices];
		static_assert(k_totalRootSigDescriptorTableIndices == (sizeof(m_rootSigDescriptorTableIdxBitmask) * 8));

	private:
		std::unordered_map<std::string, RootSigEntry> m_namesToRootEntries;
	};
}