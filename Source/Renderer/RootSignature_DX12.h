// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include <wrl.h>
#include <d3d12.h>
#include <d3d12shader.h>

struct CD3DX12_ROOT_PARAMETER1;

namespace re
{
	class Sampler;
	class Shader;
}

namespace dx12
{
	class RootSignature final
	{
	public:
		static constexpr uint32_t k_maxRootSigEntries = 64;

		static constexpr uint8_t k_invalidRootSigIndex	= std::numeric_limits<uint8_t>::max();
		static constexpr uint8_t k_invalidOffset		= std::numeric_limits<uint8_t>::max();
		static constexpr uint32_t k_invalidCount		= std::numeric_limits<uint32_t>::max();

		static constexpr uint32_t k_invalidRegisterVal	= std::numeric_limits<uint32_t>::max();


	public: // Descriptor table metadata:
		struct RangeEntry
		{
			// No. of descriptors bound to the same name (e.g. for arrays of buffers)
			uint32_t m_bindCount = std::numeric_limits<uint32_t>::max(); // -1 == unbounded size

			uint32_t m_baseRegister = k_invalidRegisterVal; 
			uint32_t m_registerSpace = k_invalidRegisterVal;

			D3D12_DESCRIPTOR_RANGE_FLAGS m_flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

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
		struct DescriptorTable // Describes the layout of a descriptor table: [[SRVs], [UAVs], [CBVs]]
		{
			uint8_t m_index = k_invalidRootSigIndex; // All individual RootParameters in this table have the same index
			std::array<std::vector<RangeEntry>, DescriptorType::Type_Count> m_ranges; // A vector of RangeEntry for each DescriptorType

			D3D12_SHADER_VISIBILITY m_visibility = D3D12_SHADER_VISIBILITY_ALL;
		};


	public: // Binding metadata:
		struct RootConstant
		{
			uint32_t m_num32BitValues = k_invalidCount;
		};
		struct RootCBV
		{
			D3D12_ROOT_DESCRIPTOR_FLAGS m_flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		};
		struct RootSRV
		{
			D3D12_SRV_DIMENSION m_viewDimension;
			D3D12_ROOT_DESCRIPTOR_FLAGS m_flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		};
		struct RootUAV
		{
			D3D12_UAV_DIMENSION m_viewDimension;
			D3D12_ROOT_DESCRIPTOR_FLAGS m_flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		};
		struct TableEntry // Describes an individual (named) resource packed in a descriptor table
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

			uint32_t m_registerBindPoint = k_invalidRegisterVal;
			uint32_t m_registerSpace = k_invalidRegisterVal;

			D3D12_SHADER_VISIBILITY m_visibility = D3D12_SHADER_VISIBILITY_ALL;

			union
			{
				RootConstant m_rootConstant;
				RootCBV m_rootCBV;
				RootSRV m_rootSRV;
				RootUAV m_rootUAV;
				TableEntry m_tableEntry;
			};
		};


	public: // Create a root signature from shader reflection:
		[[nodiscard]] static std::unique_ptr<dx12::RootSignature> Create(re::Shader const&);


	public: // Manual root signature creation:
		[[nodiscard]] static std::unique_ptr<dx12::RootSignature> CreateUninitialized();


		struct RootParameterCreateDesc
		{
			std::string m_shaderName;
			RootParameter::Type m_type = RootParameter::Type::Type_Invalid;
			uint32_t m_registerBindPoint = 0;
			uint32_t m_registerSpace = 0;
			D3D12_ROOT_DESCRIPTOR_FLAGS m_flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE; // Volatile = root sig 1.0 default
			D3D12_SHADER_VISIBILITY m_visibility = D3D12_SHADER_VISIBILITY_ALL;

			union
			{
				D3D12_SRV_DIMENSION m_srvViewDimension;
				D3D12_UAV_DIMENSION m_uavViewDimension;
				uint8_t m_numRootConstants = 0;
			};
		};
		uint32_t AddRootParameter(RootParameterCreateDesc const&); // Returns the index of the new root parameter


		struct DescriptorRangeCreateDesc
		{
			std::string m_shaderName;

			D3D12_DESCRIPTOR_RANGE1 m_rangeDesc;

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
		uint32_t AddDescriptorTable(
			std::vector<DescriptorRangeCreateDesc> const&,
			D3D12_SHADER_VISIBILITY = D3D12_SHADER_VISIBILITY_ALL);

		void AddStaticSampler(core::InvPtr<re::Sampler> const&);

		void Finalize(char const* name, D3D12_ROOT_SIGNATURE_FLAGS);


	public:
		~RootSignature();
		void Destroy();


	public:
		uint64_t GetDescriptorTableIdxBitmask() const;
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
		void ValidateRootSigSize(); // _DEBUG only

		void FinalizeInternal(
			std::wstring const& rootSigName,
			std::vector<CD3DX12_ROOT_PARAMETER1> const&, 
			std::vector<D3D12_STATIC_SAMPLER_DESC> const&,
			D3D12_ROOT_SIGNATURE_FLAGS);


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
			std::vector<std::string>& staticSamplerNames,
			std::vector<D3D12_STATIC_SAMPLER_DESC>& staticSamplers);


	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
		uint64_t m_rootSigDescHash;


	private: // Track which root sig indexes contain descriptor tables, and how many entries they have
		uint64_t m_rootSigDescriptorTableIdxBitmask; 
		uint32_t m_numDescriptorsPerTable[k_maxRootSigEntries];
		SEStaticAssert(k_maxRootSigEntries == (sizeof(m_rootSigDescriptorTableIdxBitmask) * 8),
			"Not enough bits in the m_rootSigDescriptorTableIdxBitmask to represent all root signature entries");


	private: // Binding metadata
		void InsertNewRootParamMetadata(char const* name, RootParameter&&);

		// Flattened root parameter entries. 1 element per descriptor, regardless of its root/table location
		std::vector<RootParameter> m_rootParamMetadata; 
		std::unordered_map<std::string, uint32_t> m_namesToRootParamsIdx;

		std::vector<DescriptorTable> m_descriptorTables; // For null descriptor initialization

		std::vector<std::string> m_staticSamplerNames;

		bool m_isFinalized;
	};


	inline uint64_t RootSignature::GetDescriptorTableIdxBitmask() const
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
		return m_rootParamMetadata;
	}


	inline uint32_t RootSignature::GetNumRootSignatureEntries() const
	{
		return util::CheckedCast<uint32_t>(m_rootParamMetadata.size());
	}


	inline std::vector<RootSignature::DescriptorTable> const& RootSignature::GetDescriptorTableMetadata() const
	{
		return m_descriptorTables;
	}
}