// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "NamedObject.h"


namespace re
{
	/*******************************************************************************************************************
	* Parameter Blocks have 2 modification/access types:
	* 1) Mutable:		Can be modified, and are rebuffered when modification is detected
	* 2) Immutable:		Buffered once at creation, and cannot be modified
	*
	* Parameter Blocks have 2 lifetime scopes:
	* 1) Permanent:		Allocated once, and held for the lifetime of the program
	* 2) Single frame:	Allocated and destroyed within a single frame
	*					-> Single frame parameter blocks are immutable once they are committed
	*
	* The union of these gives us Permanent Mutable, Permanent Immutable, & SingleFrame Immutable Parameter Block types
	*******************************************************************************************************************/

	class ParameterBlock : public virtual en::NamedObject
	{
	public:
		enum PBType : uint8_t
		{
			Mutable,		// Permanent, can be updated
			Immutable,		// Permanent, cannot be updated
			SingleFrame,	// Single frame, immutable once committed

			PBType_Count
		};

		enum PBDataType : uint8_t
		{
			SingleElement,
			Array,

			PBDataType_Count
		};


	public:
		struct PlatformParams : public re::IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false;

			PBDataType m_dataType = PBDataType::PBDataType_Count;
			uint32_t m_numElements = 0;
		};


	public: // Parameter block factories:

		// Create a PB for a single data object (eg. stage parameter block)
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::ParameterBlock> Create(
			std::string const& pbName, T const& data, PBType pbType);

		// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
		template<typename T>
		static std::shared_ptr<re::ParameterBlock> CreateFromArray(
			std::string const& pbName, T const* dataArray, uint32_t dataByteSize, uint32_t numElements, PBType pbType);

		ParameterBlock(ParameterBlock&&) = default;
		ParameterBlock& operator=(ParameterBlock&&) = default;
		~ParameterBlock();

		void Destroy();


	public:
		template <typename T>
		void Commit(T const& data); // Commit *updated* data
		
		template <typename T>
		void Commit(T const* data, uint32_t baseIdx, uint32_t numElements); // Recommit mutable array data (only)
	
		void GetDataAndSize(void const*& out_data, uint32_t& out_numBytes) const;
		uint32_t GetSize() const;
		uint32_t GetStride() const;
		inline PBType GetType() const { return m_pbType; }

		uint32_t GetNumElements() const; // Instanced ParameterBlocks: How many instances of data does the PB hold?

		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }

	private:		
		uint64_t m_typeIDHash; // Hash of the typeid(T) at Create: Used to verify committed data types don't change

		const PBType m_pbType;

		std::unique_ptr<PlatformParams> m_platformParams;
		

	private:
		// Use the factory Create() method instead
		ParameterBlock(size_t typeIDHashCode, std::string const& pbName, PBType, PBDataType, uint32_t numElements); 

		static void RegisterAndCommit(
			std::shared_ptr<re::ParameterBlock> newPB, void const* data, uint32_t numBytes, uint64_t typeIDHash);
		
		void CommitInternal(void const* data, uint64_t typeIDHash);

		void CommitInternal(void const* data, uint32_t baseOffset, uint32_t numBytes, uint64_t typeIDHash); // Partial


	private:
		ParameterBlock() = delete;
		ParameterBlock(ParameterBlock const&) = delete;
		ParameterBlock& operator=(ParameterBlock const&) = delete;
	};


	// Create a PB for a single data object (eg. stage parameter block)
	template<typename T>
	std::shared_ptr<re::ParameterBlock> ParameterBlock::Create(std::string const& pbName, T const& data, PBType pbType)
	{
		std::shared_ptr<re::ParameterBlock> newPB;
		newPB.reset(new re::ParameterBlock(typeid(T).hash_code(), pbName, pbType, PBDataType::SingleElement, 1));

		RegisterAndCommit(newPB, &data, sizeof(T), typeid(T).hash_code());

		return newPB;
	}


	// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
	template<typename T>
	static std::shared_ptr<re::ParameterBlock> ParameterBlock::CreateFromArray(
		std::string const& pbName, T const* dataArray, uint32_t dataByteSize, uint32_t numElements, PBType pbType)
	{
		std::shared_ptr<re::ParameterBlock> newPB;
		newPB.reset(new ParameterBlock(
			typeid(T).hash_code(), 
			pbName, 
			pbType, 
			PBDataType::Array, 
			static_cast<uint32_t>(numElements)));

		RegisterAndCommit(newPB, dataArray, dataByteSize * numElements, typeid(T).hash_code());

		return newPB;
	}


	template <typename T>
	void ParameterBlock::Commit(T const& data) // Commit *updated* data
	{
		CommitInternal(&data, typeid(T).hash_code());
	}


	template <typename T>
	void ParameterBlock::Commit(T const* data, uint32_t baseIdx, uint32_t numElements)
	{
		const uint32_t dstBaseByteOffset = baseIdx * sizeof(T);
		const uint32_t numBytes = numElements * sizeof(T);

		CommitInternal(data, dstBaseByteOffset, numBytes, typeid(T).hash_code());
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline ParameterBlock::PlatformParams::~PlatformParams() {};
}