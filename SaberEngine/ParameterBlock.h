#pragma once

#include <memory>
#include <string>

#include "ParameterBlock_Platform.h"
#include "NamedObject.h"
#include "DebugConfiguration.h"

namespace re
{
	class ParameterBlockManager;
}

namespace re
{
	class ParameterBlock : public virtual en::NamedObject
	{
	public:
		typedef uint64_t Handle; // == NamedObject::UniqueID()
		static const uint64_t k_invalidPBHandle = static_cast<uint64_t>(-1);

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
		static Handle Create(std::string pbName, T const& data, UpdateType updateType, Lifetime lifetime);

		template<typename T> 
		static Handle CreateSingleFrame(std::string pbName, T const& data);


		// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
		template<typename T>
		static Handle CreateFromArray(
			std::string pbName, T const* dataArray, size_t dataByteSize, size_t numElements, UpdateType updateType, Lifetime lifetime);

		template<typename T>
		static Handle CreateFromArraySingleFrame(
			std::string pbName, T const* dataArray, size_t dataByteSize, size_t numElements);


		inline void const* const GetData() const { return m_dataCopy.get(); }
		size_t GetDataSize() const { return m_dataSizeInBytes; }

		inline UpdateType GetUpdateType() const { return m_updateType; }
		inline Lifetime GetLifetime() const { return m_lifetime; }

		bool GetDirty() const { return m_isDirty; }
		void MarkClean() { m_isDirty = false; }

		~ParameterBlock() { Destroy(); };

		inline platform::ParameterBlock::PlatformParams* const GetPlatformParams() const { return m_platformParams.get(); }


	private:
		template <typename T>
		void SetData(T const& data); // Called from ParameterBlockManager

		static ParameterBlock::Handle Register(std::shared_ptr<re::ParameterBlock> newPB);
		struct Accessor { explicit Accessor() = default; }; // Prevents direct access to the CTOR
	public:
		template <typename T>
		ParameterBlock(Accessor, std::string pbName, std::shared_ptr<T> dataCopy, size_t dataByteSize, UpdateType updateType, Lifetime lifetime);
		

	private:		
		std::shared_ptr<void> m_dataCopy;
		const size_t m_dataSizeInBytes;
		uint64_t m_typeIDHashCode; // Hash of the typeid(T) at Create: Used to verify data type doesn't change

		const UpdateType m_updateType;
		bool m_isDirty;

		const Lifetime m_lifetime;

		std::unique_ptr<platform::ParameterBlock::PlatformParams> m_platformParams;
		
		void Destroy();

	private:
		ParameterBlock() = delete;
		ParameterBlock(ParameterBlock const&) = delete;
		ParameterBlock(ParameterBlock&&) = delete;
		ParameterBlock& operator=(ParameterBlock const&) = delete;

		// Friends:
		friend void platform::ParameterBlock::PlatformParams::CreatePlatformParams(re::ParameterBlock&);
		friend class re::ParameterBlockManager;
	};


	// Pseudo-private CTOR: private ParameterBlock::Accessor forces access via one of the Create factories
	template <typename T>
	ParameterBlock::ParameterBlock(
		ParameterBlock::Accessor, std::string pbName, std::shared_ptr<T> dataCopy, size_t dataByteSize, UpdateType updateType, Lifetime lifetime)
		: NamedObject(pbName)
		, m_dataCopy(dataCopy)
		, m_dataSizeInBytes(dataByteSize)
		, m_typeIDHashCode(typeid(T).hash_code())
		, m_updateType(updateType)
		, m_isDirty(true)
		, m_lifetime(lifetime)
	{
		platform::ParameterBlock::PlatformParams::CreatePlatformParams(*this);
		platform::ParameterBlock::Create(*this);
	}


	// Create a PB for a single data object (eg. stage parameter block)
	template<typename T>
	ParameterBlock::Handle ParameterBlock::Create(
		std::string pbName, T const& data, UpdateType updateType, Lifetime lifetime)
	{
		std::shared_ptr<T> dataCopy = std::make_shared<T>(data);
		std::shared_ptr<re::ParameterBlock> newPB = 
			make_shared<re::ParameterBlock>(Accessor(), pbName, dataCopy, sizeof(T), updateType, lifetime);
		return Register(newPB);
	}


	template<typename T>
	ParameterBlock::Handle ParameterBlock::CreateSingleFrame(std::string pbName, T const& data)
	{
		return Create(pbName, data, ParameterBlock::UpdateType::Immutable, ParameterBlock::Lifetime::SingleFrame);
	}


	// Create a PB for an array of several objects of the same type (eg. instanced mesh matrices)
	template<typename T>
	static ParameterBlock::Handle ParameterBlock::CreateFromArray(
		std::string pbName, T const* dataArray, size_t dataByteSize, size_t numElements, UpdateType updateType, Lifetime lifetime)
	{
		std::shared_ptr<T> dataCopy(new T[numElements], [](T* t) {delete[] t; });
		memcpy(dataCopy.get(), dataArray, dataByteSize * numElements);

		std::shared_ptr<re::ParameterBlock> newPB =
			make_shared<ParameterBlock>(Accessor(), pbName, dataCopy, dataByteSize * numElements, updateType, lifetime);

		return Register(newPB);
	}


	template<typename T>
	ParameterBlock::Handle ParameterBlock::CreateFromArraySingleFrame(
		std::string pbName, T const* dataArray, size_t dataByteSize, size_t numElements)
	{
		return CreateFromArray(pbName, dataArray, dataByteSize, numElements, 
			ParameterBlock::UpdateType::Immutable, ParameterBlock::Lifetime::SingleFrame);
	}


	template <typename T>
	void ParameterBlock::SetData(T const& data)
	{
		SEAssert("Invalid type detected. Can only set data of the original type",
			typeid(T).hash_code() == m_typeIDHashCode);
		SEAssert("Cannot set data of an immutable param block", m_updateType != UpdateType::Immutable);

		m_dataCopy = std::make_shared<T>(data);
		m_isDirty = true;
	}
}