#pragma once

#include <memory>
#include <string>

#include "ParameterBlock_Platform.h"
#include "NamedObject.h"
#include "DebugConfiguration.h"


namespace re
{
	class ParameterBlock : public virtual en::NamedObject
	{
	public:
		enum class UpdateType
		{
			Mutable,	// Data can be updated per frame
			Immutable,	// Allocated/buffered at Create, and deallocated/destroyed when the destructor is called

			UpdateFrequency_Count
		};

		enum class Lifetime
		{
			Permanent,
			SingleFrame,

			Lifetime_Count
		};


	public: // Parameter block factories:

		// Create a PB for a single data object (eg. stage parameter block)
		template<typename T>
		static std::shared_ptr<re::ParameterBlock> Create(
			std::string pbName, T const& data, UpdateType updateType, Lifetime lifetime);

		
		// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
		template<typename T>
		static std::shared_ptr<re::ParameterBlock> CreateFromArray(
			std::string pbName, T const* dataArray, size_t dataByteSize, size_t numElements, UpdateType updateType, Lifetime lifetime);


	private:
		struct Accessor { explicit Accessor() = default; }; // Prevents direct access to the CTOR
	public:
		ParameterBlock(Accessor, size_t typeIDHashCode, std::string pbName, UpdateType updateType, Lifetime lifetime);

		~ParameterBlock() { Destroy(); };

	public:
		template <typename T>
		void Commit(T const& data);
	
		void GetDataAndSize(void*& out_data, size_t& out_numBytes);
	
		inline UpdateType GetUpdateType() const { return m_updateType; }
		inline Lifetime GetLifetime() const { return m_lifetime; }

		bool GetDirty() const { return m_isDirty; }
		void MarkClean() { m_isDirty = false; }

		inline platform::ParameterBlock::PlatformParams* const GetPlatformParams() const { return m_platformParams.get(); }

	private:		
		uint64_t m_typeIDHash; // Hash of the typeid(T) at Create: Used to verify committed data types don't change

		const Lifetime m_lifetime;
		const UpdateType m_updateType;

		bool m_isDirty;

		std::unique_ptr<platform::ParameterBlock::PlatformParams> m_platformParams;
		

	private:
		static void RegisterAndCommit(std::shared_ptr<re::ParameterBlock> newPB, void const* data, size_t numBytes);
		void CommitInternal(void const* data, uint64_t typeIDHash);

		void Destroy();


	private:
		ParameterBlock() = delete;
		ParameterBlock(ParameterBlock const&) = delete;
		ParameterBlock(ParameterBlock&&) = delete;
		ParameterBlock& operator=(ParameterBlock const&) = delete;

		// Friends:
		friend void platform::ParameterBlock::PlatformParams::CreatePlatformParams(re::ParameterBlock&);
	};


	// Create a PB for a single data object (eg. stage parameter block)
	template<typename T>
	std::shared_ptr<re::ParameterBlock> ParameterBlock::Create(
		std::string pbName, T const& data, UpdateType updateType, Lifetime lifetime)
	{
		std::shared_ptr<re::ParameterBlock> newPB =
			make_shared<re::ParameterBlock>(Accessor(), typeid(T).hash_code(), pbName, updateType, lifetime);

		RegisterAndCommit(newPB, &data, sizeof(T));

		return newPB;
	}


	// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
	template<typename T>
	static std::shared_ptr<re::ParameterBlock> ParameterBlock::CreateFromArray(
		std::string pbName, T const* dataArray, size_t dataByteSize, size_t numElements, UpdateType updateType, Lifetime lifetime)
	{
		std::shared_ptr<re::ParameterBlock> newPB =
			make_shared<ParameterBlock>(Accessor(), typeid(T).hash_code(), pbName, updateType, lifetime);

		RegisterAndCommit(newPB, dataArray, dataByteSize * numElements);

		return newPB;
	}


	template <typename T>
	void ParameterBlock::Commit(T const& data)
	{
		CommitInternal(&data, typeid(T).hash_code());
	}
}