// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/Core/Assert.h"

#include "Private/Core/Util/HashKey.h"
#include "Private/Core/Util/HashUtils.h"


namespace core
{
	class IHashedDataObject
	{
	protected:
		virtual void ComputeDataHash() = 0; // Should be called once the implementer is fully initialized

	public:
		IHashedDataObject() : m_dataHash(0) {}
		
		virtual ~IHashedDataObject() = default;


	public:
		virtual util::HashKey GetDataHash() const;

		void AddDataBytesToHash(void const* const data, size_t numBytes);

		void AddDataBytesToHash(std::string const& str);

		template<typename T>
		void AddDataBytesToHash(T const& data);

		template<typename T>
		void AddDataBytesToHash(std::vector<T> const& dataVec);

		void ResetDataHash();

		void SetDataHash(util::HashKey); // Set the data hash using a precomputed value


	private:
		util::HashKey m_dataHash;
	};


	inline util::HashKey IHashedDataObject::GetDataHash() const
	{
		return m_dataHash;
	}


	inline void IHashedDataObject::AddDataBytesToHash(void const* const data, size_t numBytes)
	{
		SEAssert(data != nullptr && numBytes > 0, "Invalid data for hash");

		util::CombineHash(m_dataHash, util::HashDataBytes(data, numBytes));
	}


	inline void IHashedDataObject::AddDataBytesToHash(std::string const& str)
	{
		AddDataBytesToHash(std::hash<std::string>{}(str));
	}


	template<typename T>
	inline void IHashedDataObject::AddDataBytesToHash(T const& data)
	{
		AddDataBytesToHash(&data, sizeof(T));
	}


	template<typename T>
	inline void IHashedDataObject::AddDataBytesToHash(std::vector<T> const& dataVec)
	{
		for (T const& t : dataVec)
		{
			AddDataBytesToHash(t);
		}
	}


	inline void IHashedDataObject::ResetDataHash()
	{
		m_dataHash = 0;
	}


	inline void IHashedDataObject::SetDataHash(util::HashKey dataHash)
	{
		m_dataHash = dataHash;
	}
}