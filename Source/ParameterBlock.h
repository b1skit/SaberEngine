// © 2022 Adam Badke. All rights reserved.
#pragma once

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
	* The union of these properties give us Immutable, Mutable, and SingleFrame Parameter Block types
	*******************************************************************************************************************/

	class ParameterBlock : public virtual en::NamedObject
	{
	public:
		struct PlatformParams
		{
			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams() = default;
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			// API-specific GPU bindings should be destroyed here
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false;
		};


	public:
		enum class PBType
		{
			Mutable,		// Permanent, can be updated
			Immutable,		// Permanent, cannot be updated
			SingleFrame,	// Single frame, immutable once committed

			PBType_Count
		};


	public: // Parameter block factories:

		// Create a PB for a single data object (eg. stage parameter block)
		template<typename T>
		static std::shared_ptr<re::ParameterBlock> Create(std::string const& pbName, T const& data, PBType pbType);

		// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
		template<typename T>
		static std::shared_ptr<re::ParameterBlock> CreateFromArray(
			std::string const& pbName, T const* dataArray, size_t dataByteSize, size_t numElements, PBType pbType);


	public:
		template <typename T>
		void Commit(T const& data); // Commit *updated* data
	
		void GetDataAndSize(void*& out_data, size_t& out_numBytes);
		inline PBType GetType() const { return m_pbType; }

		inline PlatformParams* const GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }

	private:		
		uint64_t m_typeIDHash; // Hash of the typeid(T) at Create: Used to verify committed data types don't change

		const PBType m_pbType;

		std::unique_ptr<PlatformParams> m_platformParams;
		

	private:
		struct Accessor { explicit Accessor() = default; }; // Prevents direct access to the CTOR
	public:
		ParameterBlock(Accessor, size_t typeIDHashCode, std::string const& pbName, PBType pbType);

		~ParameterBlock() { Destroy(); };


	private:
		static void RegisterAndCommit(
			std::shared_ptr<re::ParameterBlock> newPB, void const* data, size_t numBytes, uint64_t typeIDHash);
		void CommitInternal(void const* data, uint64_t typeIDHash);

		void Destroy();


	private:
		ParameterBlock() = delete;
		ParameterBlock(ParameterBlock const&) = delete;
		ParameterBlock(ParameterBlock&&) = delete;
		ParameterBlock& operator=(ParameterBlock const&) = delete;
	};


	// Create a PB for a single data object (eg. stage parameter block)
	template<typename T>
	std::shared_ptr<re::ParameterBlock> ParameterBlock::Create(std::string const& pbName, T const& data, PBType pbType)
	{
		std::shared_ptr<re::ParameterBlock> newPB =
			make_shared<re::ParameterBlock>(Accessor(), typeid(T).hash_code(), pbName, pbType);

		RegisterAndCommit(newPB, &data, sizeof(T), typeid(T).hash_code());

		return newPB;
	}


	// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
	template<typename T>
	static std::shared_ptr<re::ParameterBlock> ParameterBlock::CreateFromArray(
		std::string const& pbName, T const* dataArray, size_t dataByteSize, size_t numElements, PBType pbType)
	{
		std::shared_ptr<re::ParameterBlock> newPB =
			make_shared<ParameterBlock>(Accessor(), typeid(T).hash_code(), pbName, pbType);

		RegisterAndCommit(newPB, dataArray, dataByteSize * numElements, typeid(T).hash_code());

		return newPB;
	}


	template <typename T>
	void ParameterBlock::Commit(T const& data) // Commit *updated* data
	{
		CommitInternal(&data, typeid(T).hash_code());
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline ParameterBlock::PlatformParams::~PlatformParams() {};
}