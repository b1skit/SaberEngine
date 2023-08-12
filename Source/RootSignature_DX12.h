// © 2023 Adam Badke. All rights reserved.
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
		static constexpr uint32_t k_totalRootSigDescriptorTableIndices = 32u;

		static constexpr uint8_t k_invalidRootSigIndex	= std::numeric_limits<uint8_t>::max();
		static constexpr uint8_t k_invalidOffset		= std::numeric_limits<uint8_t>::max();
		static constexpr uint8_t k_invalidCount			= std::numeric_limits<uint8_t>::max();

		static constexpr uint8_t k_invalidRegisterVal	= std::numeric_limits<uint8_t>::max();


	public: // Descriptor table metadata:
		struct RangeEntry
		{
			union
			{
				struct
				{
					uint32_t m_sizeInBytes;
				} m_cbvDesc;
				struct
				{
					DXGI_FORMAT m_format;
					D3D12_SRV_DIMENSION m_viewDimension;
				} m_srvDesc;
				struct
				{
					DXGI_FORMAT m_format;
					D3D12_UAV_DIMENSION m_viewDimension;
				} m_uavDesc;
			};
		};
		enum DescriptorType : uint8_t
		{
			SRV,
			UAV,
			CBV,
			// Note: Sampler type is omitted

			Type_Count,
			Type_Invalid = Type_Count
		};
		struct Range
		{
			DescriptorType m_type = DescriptorType::Type_Invalid;

			std::vector<RangeEntry> m_rangeEntries;
		};
		struct DescriptorTable
		{
			DescriptorTable() { m_ranges.resize(DescriptorType::Type_Count); }
			
			uint8_t m_index = k_invalidRootSigIndex;
			std::vector<std::vector<RangeEntry>> m_ranges;
		};


	public: // Binding metadata:
		struct RootConstant
		{
			uint8_t m_num32BitValues = k_invalidCount;
			uint8_t m_destOffsetIn32BitValues = k_invalidOffset; // TODO: Is this needed/used?
		};
		struct TableEntry
		{
			DescriptorType m_type = DescriptorType::Type_Invalid;
			uint8_t m_offset = k_invalidOffset;
		};
		struct RootParameter
		{
			uint8_t m_index = k_invalidRootSigIndex;

			enum class Type
			{
				Constant,
				CBV,
				SRV,
				UAV,
				DescriptorTable,

				Type_Count,
				Type_Invalid = Type_Count
			} m_type = Type::Type_Invalid;

			uint8_t m_registerBindPoint = k_invalidRegisterVal;
			uint8_t m_registerSpace = k_invalidRegisterVal;

			union
			{
				RootConstant m_rootConstant;
				TableEntry m_tableEntry;
			};
		};


	public:
		static std::shared_ptr<dx12::RootSignature> Create(re::Shader const&);

		~RootSignature();
		void Destroy();

		uint32_t GetDescriptorTableIdxBitmask() const;
		uint32_t GetNumDescriptorsInTable(uint8_t rootIndex) const;

		ID3D12RootSignature* GetD3DRootSignature() const;

		uint64_t GetRootSigDescHash() const;

		std::vector<RootParameter> const& GetRootSignatureEntries() const;

		RootParameter const* GetRootSignatureEntry(std::string const& resourceName) const;
		RootParameter const* GetRootSignatureEntry(DescriptorType, uint8_t registerBindPoint) const;

		bool HasResource(std::string const& resourceName) const;


		std::vector<DescriptorTable> const& GetDescriptorTableMetadata() const;


	private:
		RootSignature();


	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

		uint64_t m_rootSigDescHash;


	private: // Track which root sig indexes contain descriptor tables, and how many entries they have
		uint32_t m_rootSigDescriptorTableIdxBitmask; 
		uint32_t m_numDescriptorsPerTable[k_totalRootSigDescriptorTableIndices];
		static_assert(k_totalRootSigDescriptorTableIndices == (sizeof(m_rootSigDescriptorTableIdxBitmask) * 8));

	private: // Binding metadata
		void InsertNewRootParamMetadata(char const* name, RootParameter&&);

		std::vector<RootParameter> m_rootParams; // 1 entry for each descriptor, regardless of its root/table location
		std::unordered_map<std::string, size_t> m_namesToRootParamsIdx;
		std::array<std::unordered_map<uint8_t, size_t>, DescriptorType::Type_Count> m_registerToRootParamIdx;

		std::vector<DescriptorTable> m_descriptorTables; // For null descriptor initialization
	};


	inline uint64_t RootSignature::GetRootSigDescHash() const
	{
		return m_rootSigDescHash;
	}


	inline std::vector<RootSignature::RootParameter> const& RootSignature::GetRootSignatureEntries() const
	{
		return m_rootParams;
	}


	inline std::vector<RootSignature::DescriptorTable> const& RootSignature::GetDescriptorTableMetadata() const
	{
		return m_descriptorTables;
	}
}