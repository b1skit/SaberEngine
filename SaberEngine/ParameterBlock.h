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

	public: 
		template<typename T>
		static std::shared_ptr<re::ParameterBlock> Create(
			std::string paramBlockName, 
			std::shared_ptr<T> data, 
			UpdateType updateType,
			Lifetime lifetime)
		{
			std::shared_ptr<re::ParameterBlock> newPB = make_shared<re::ParameterBlock>(
				Accessor(), paramBlockName, data, sizeof(T), updateType, lifetime);
			Register(newPB);
			return newPB;
		}

	private:
		static void Register(std::shared_ptr<re::ParameterBlock> newPB);
		struct Accessor { explicit Accessor() = default; }; // Prevents direct access to the CTOR
	public:
		template <typename T>
		ParameterBlock(
			ParameterBlock::Accessor,
			std::string pbShaderName,
			std::shared_ptr<T> data,
			size_t dataSizeInBytes,
			UpdateType updateType,
			Lifetime lifetime) :
				NamedObject(pbShaderName),
			m_data(data),
			m_dataSizeInBytes(dataSizeInBytes),
			m_typeIDHashCode(typeid(T).hash_code()),
			m_updateType(updateType),
			m_isDirty(true),
			m_lifetime(lifetime)
		{
			platform::ParameterBlock::PlatformParams::CreatePlatformParams(*this);
			platform::ParameterBlock::Create(*this);
		}

		~ParameterBlock() { Destroy(); };

		template <typename T>
		void SetData(std::shared_ptr<T> data)
		{
			SEAssert("Invalid type detected. Can only set data of the original type", 
				typeid(T).hash_code() == m_typeIDHashCode);
			SEAssert("Cannot set data of an immutable param block", m_updateType != UpdateType::Immutable);

			m_data = data;
			m_isDirty = true;
		}
	
	public:
		inline void const* const GetData() const { return m_data.get(); }
		size_t GetDataSize() const { return m_dataSizeInBytes; }

		inline UpdateType GetUpdateType() const { return m_updateType; }
		inline Lifetime GetLifetime() const { return m_lifetime; }

		bool GetDirty() const { return m_isDirty; }
		void MarkClean() { m_isDirty = false; }

		inline platform::ParameterBlock::PlatformParams* const GetPlatformParams() const { return m_platformParams.get(); }

	private:		
		std::shared_ptr<void> m_data;
		const size_t m_dataSizeInBytes;
		size_t m_typeIDHashCode; // Hash of the typeid(T) at Create: Used to verify data type doesn't change

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
	};
}