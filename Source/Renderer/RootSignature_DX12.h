// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include "Core/Interfaces/IHashedDataObject.h"

#include <wrl.h>
#include <d3d12.h>
#include <d3d12shader.h>

struct CD3DX12_ROOT_PARAMETER1;

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
			// No. of descriptors bound to the same name (e.g. for arrays of buffers)
			uint32_t m_bindCount = std::numeric_limits<uint32_t>::max();

			union
			{
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
		enum DescriptorType : uint8_t // Entries stored in a descriptor table
		{
			SRV,
			UAV,
			CBV,
			// Note: Sampler type is omitted

			Type_Count,
			Type_Invalid = Type_Count
		};
		struct DescriptorTable
		{
			uint8_t m_index = k_invalidRootSigIndex;
			std::array<std::vector<RangeEntry>, DescriptorType::Type_Count> m_ranges; // A vector of RangeEntry for each DescriptorType
		};


	public: // Binding metadata:
		struct RootConstant
		{
			uint8_t m_num32BitValues = k_invalidCount;
			uint8_t m_destOffsetIn32BitValues = k_invalidOffset; // TODO: Is this needed/used?
		};
		struct RootCBV
		{
			// TODO...
		};
		struct RootSRV
		{
			D3D12_SRV_DIMENSION m_viewDimension;
		};
		struct RootUAV
		{
			D3D12_UAV_DIMENSION m_viewDimension;
		};
		struct TableEntry
		{
			DescriptorType m_type = DescriptorType::Type_Invalid;
			uint8_t m_offset = k_invalidOffset;

			union
			{
				D3D12_SRV_DIMENSION m_srvViewDimension;
				D3D12_UAV_DIMENSION m_uavViewDimension;
			};
		};
		struct RootParameter
		{
			uint8_t m_index = k_invalidRootSigIndex; // Root signature index. Table entries have the same index

			enum class Type // Entries stored directly in the root signature
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
				RootCBV m_rootCBV;
				RootSRV m_rootSRV;
				RootUAV m_rootUAV;
				TableEntry m_tableEntry;
			};
		};


	public:
		[[nodiscard]] static std::unique_ptr<dx12::RootSignature> Create(re::Shader const&);

		~RootSignature();
		void Destroy();

	public:
		uint32_t GetDescriptorTableIdxBitmask() const;
		uint32_t GetNumDescriptorsInTable(uint8_t rootIndex) const;

		ID3D12RootSignature* GetD3DRootSignature() const;

		uint64_t GetRootSigDescHash() const;

		std::vector<RootParameter> const& GetRootSignatureEntries() const;
		uint32_t GetNumRootSignatureEntries() const;

		RootParameter const* GetRootSignatureEntry(std::string const& resourceName) const;

		std::vector<DescriptorTable> const& GetDescriptorTableMetadata() const; // E.g. For pre-setting null descriptors


	public: // Debug-only helpers:
#if defined(_DEBUG)
		bool HasResource(std::string const& resourceName) const;
		std::string const& DebugGetNameFromRootParamIdx(uint8_t) const;
#endif


	private: 
		RootSignature(); // Use Create() instead


	private: // Create() helpers:
		struct RangeInput : public D3D12_SHADER_INPUT_BIND_DESC
		{
			// We inherit from D3D12_SHADER_INPUT_BIND_DESC so we can store the visibility, and store the .Name in a
			// std::string (as D3D12_SHADER_INPUT_BIND_DESC::Name is released when the 
			// ID3D12LibraryReflection/ID3D12ShaderReflection go out of scope
			std::string m_name;
			D3D12_SHADER_VISIBILITY m_visibility;
		};
		static void ParseInputBindingDesc(
			dx12::RootSignature*,
			re::Shader::ShaderType,
			D3D12_SHADER_INPUT_BIND_DESC const&,
			std::array<std::vector<RangeInput>, DescriptorType::Type_Count>& rangeInputs,
			std::vector<CD3DX12_ROOT_PARAMETER1>& rootParameters,
			std::vector<D3D12_STATIC_SAMPLER_DESC>& staticSamplers);


	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
		uint64_t m_rootSigDescHash;


	private: // Track which root sig indexes contain descriptor tables, and how many entries they have
		uint32_t m_rootSigDescriptorTableIdxBitmask; 
		uint32_t m_numDescriptorsPerTable[k_totalRootSigDescriptorTableIndices];
		static_assert(k_totalRootSigDescriptorTableIndices == (sizeof(m_rootSigDescriptorTableIdxBitmask) * 8));

	private: // Binding metadata
		void InsertNewRootParamMetadata(char const* name, RootParameter&&);

		// Flattened root parameter entries. 1 element per descriptor, regardless of its root/table location
		std::vector<RootParameter> m_rootParams; 
		std::unordered_map<std::string, size_t> m_namesToRootParamsIdx;

		std::vector<DescriptorTable> m_descriptorTables; // For null descriptor initialization
	};


	inline uint32_t RootSignature::GetDescriptorTableIdxBitmask() const
	{
		return m_rootSigDescriptorTableIdxBitmask;
	}


	inline uint32_t RootSignature::GetNumDescriptorsInTable(uint8_t rootIndex) const
	{
		return m_numDescriptorsPerTable[rootIndex];
	}


	inline ID3D12RootSignature* RootSignature::GetD3DRootSignature() const
	{
		return m_rootSignature.Get();
	}


	inline uint64_t RootSignature::GetRootSigDescHash() const
	{
		return m_rootSigDescHash;
	}


	inline std::vector<RootSignature::RootParameter> const& RootSignature::GetRootSignatureEntries() const
	{
		return m_rootParams;
	}


	inline uint32_t RootSignature::GetNumRootSignatureEntries() const
	{
		return util::CheckedCast<uint32_t>(m_rootParams.size());
	}


	inline std::vector<RootSignature::DescriptorTable> const& RootSignature::GetDescriptorTableMetadata() const
	{
		return m_descriptorTables;
	}
}