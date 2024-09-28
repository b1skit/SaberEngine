// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "../Assert.h"

#include "CastUtils.h"


namespace util
{
	class ByteVector
	{
	public:
		template<typename T>
		static ByteVector Create();

		template<typename T>
		static ByteVector Create(size_t numElements);

		template<typename T>
		static ByteVector Create(size_t numElements, T const& initialVal);

		template<typename T>
		static ByteVector Create(std::initializer_list<T>);

		enum class CloneMode
		{
			Empty,
			Allocate,
			AllocateAndCopy
		};
		static ByteVector Clone(ByteVector const&, CloneMode);

		~ByteVector() = default;

		ByteVector(ByteVector const&) = default;
		ByteVector(ByteVector&&) = default;

		ByteVector& operator=(ByteVector const&) = default;
		ByteVector& operator=(ByteVector&&) = default;


	public: // std::vector wrappers:
		template<typename T>
		void emplace_back(T const&);

		void reserve(size_t numElements);

		void resize(size_t numElements);

		template<typename T>
		void resize(size_t numElements, T const&);

		void clear();
		
		template<typename T>
		T& at(size_t elementIdx); // Can specify a different type T than construction, but no validation is performed

		template<typename T>
		T const& at(size_t elementIdx) const;

		size_t size() const;
		bool empty() const;


	public:
		size_t GetTotalNumBytes() const;
		uint8_t GetElementByteSize() const;

		std::vector<uint8_t>& data();
		std::vector<uint8_t> const& data() const;

		template<typename T>
		T* data();

		void* GetElementPtr(size_t elementIdx); // Risky: Get a raw pointer to the ith element without any type checking
		void const* GetElementPtr(size_t elementIdx) const;


	public:
		template<typename T>
		bool IsScalarType() const;

		template<typename T>
		T ScalarGetAs(size_t elementIdx) const;

		template<typename T>
		void ScalarSetFrom(size_t elementIdx, T);


	public: // Helpers:
		void Rearrange(std::vector<size_t> const& indexMap); // Shuffle elements according to the index map

		// Template/typeless updaters:
		static void CopyElement(ByteVector& dst, size_t dstElemIdx, ByteVector const& src, size_t srcElemIdx); // Element copy/overwrite
		static void EmplaceBackElement(ByteVector& dst, ByteVector const& src, size_t srcElementIdx); // Element emplacement


	private:
		ByteVector(size_t typeInfoHash, uint8_t elementByteSize);


	private:
		size_t m_typeInfoHash;
		uint8_t m_elementByteSize; // Total bytes for a single element. E.g. glm::vec2 = 2x 4B floats = 8
		std::vector<uint8_t> m_data;
	};


	template<typename T>
	inline ByteVector ByteVector::Create()
	{
		return ByteVector(typeid(T).hash_code(), sizeof(T));
	}


	template<typename T>
	inline ByteVector ByteVector::Create(size_t numElements)
	{
		ByteVector newByteVector(typeid(T).hash_code(), sizeof(T));
		newByteVector.resize(numElements);
		return newByteVector;
	}


	template<typename T>
	static ByteVector ByteVector::Create(size_t numElements, T const& initialVal)
	{
		ByteVector newByteVector(typeid(T).hash_code(), sizeof(T));
		newByteVector.resize(numElements);

		for (size_t i = 0; i < numElements; ++i)
		{
			newByteVector.at<T>(i) = initialVal;
		}

		return newByteVector;
	}


	template<typename T>
	static ByteVector ByteVector::Create(std::initializer_list<T> args)
	{
		ByteVector newByteVector(typeid(T).hash_code(), sizeof(T));
		newByteVector.reserve(args.size());

		typename std::initializer_list<T>::iterator it;
		for (it = args.begin(); it != args.end(); ++it)
		{
			newByteVector.emplace_back<T>(*it);
		}

		return newByteVector;
	}


	inline ByteVector ByteVector::Clone(ByteVector const& src, CloneMode cloneMode)
	{
		switch (cloneMode)
		{
		case CloneMode::Empty:
		{
			return ByteVector(src.m_typeInfoHash, src.m_elementByteSize);
		}
		break;
		case CloneMode::Allocate:
		{
			ByteVector clone(src.m_typeInfoHash, src.m_elementByteSize);
			clone.resize(src.size());
			return clone;
		}
		break;
		case CloneMode::AllocateAndCopy:
		{
			ByteVector clone(src.m_typeInfoHash, src.m_elementByteSize);
			clone.m_data = src.m_data;
			return clone;
		}
		break;
		default: SEAssertF("Invalid clone mode");
		}
		return ByteVector(0, 0); // This should never happen
	}


	inline ByteVector::ByteVector(size_t typeInfoHash, uint8_t elementByteSize)
		: m_typeInfoHash(typeInfoHash)
		, m_elementByteSize(elementByteSize)
	{
		SEAssert(m_elementByteSize > 0, "Invalid element size");
	}


	template<typename T>
	void ByteVector::emplace_back(T const& src)
	{
		SEAssert(m_typeInfoHash == typeid(T).hash_code(), "Type is different than what was specified at construction");

		const size_t curNumElements = size(); // m_data.size() / m_elementByteSize;
		m_data.resize(m_data.size() + m_elementByteSize);

		// Cast to our templated type, then use our curNumElements to offset to the correct location
		T* dst = reinterpret_cast<T*>(m_data.data()) + curNumElements;

		memcpy(dst, &src, m_elementByteSize);
	}


	inline void ByteVector::reserve(size_t numElements)
	{
		m_data.reserve(numElements * m_elementByteSize);
	}


	inline void ByteVector::resize(size_t numElements)
	{
		m_data.resize(numElements * m_elementByteSize);
	}


	template<typename T>
	void ByteVector::resize(size_t numElements, T const& val)
	{
		SEAssert(m_typeInfoHash == typeid(T).hash_code(), "Type is different than what was specified at construction");
		SEAssert(m_data.size() < numElements * m_elementByteSize, "Vector size is already >= requested size");

		size_t i = m_data.size();
		m_data.resize(numElements * m_elementByteSize);

		while (i < m_data.size())
		{
			memcpy(&m_data[i], &val, m_elementByteSize);
			i += m_elementByteSize;
		}
	}


	inline void ByteVector::clear()
	{
		m_data.clear();
	}


	template<typename T>
	T& ByteVector::at(size_t elementIdx)
	{
		SEAssert(m_typeInfoHash == typeid(T).hash_code(), "Type is different than what was specified at construction");

		T* element = reinterpret_cast<T*>(m_data.data()) + elementIdx;
		return *element;
	}


	template<typename T>
	T const& ByteVector::at(size_t elementIdx) const
	{
		SEAssert(m_typeInfoHash == typeid(T).hash_code(), "Type is different than what was specified at construction");

		T const* element = reinterpret_cast<T const*>(m_data.data()) + elementIdx;
		return *element;
	}


	inline size_t ByteVector::size() const
	{
		return m_data.size() / m_elementByteSize;
	}


	inline bool ByteVector::empty() const
	{
		return m_data.empty();
	}


	inline size_t ByteVector::GetTotalNumBytes() const
	{
		return m_data.size();
	}


	inline uint8_t ByteVector::GetElementByteSize() const
	{
		return m_elementByteSize;
	}


	inline std::vector<uint8_t>& ByteVector::data()
	{
		return m_data;
	}


	inline std::vector<uint8_t> const& ByteVector::data() const
	{
		return m_data;
	}


	template<typename T>
	T* ByteVector::data()
	{
		return reinterpret_cast<T*>(m_data.data());
	}


	inline void* ByteVector::GetElementPtr(size_t elementIdx)
	{
		SEAssert(elementIdx * m_elementByteSize < m_data.size(), "elementIdx is OOB");

		uint8_t* dataPtr = m_data.data();
		dataPtr += elementIdx * m_elementByteSize;
		return dataPtr;
	}


	inline void const* ByteVector::GetElementPtr(size_t elementIdx) const
	{
		SEAssert(elementIdx * m_elementByteSize < m_data.size(), "elementIdx is OOB");

		uint8_t const* dataPtr = m_data.data();
		dataPtr += elementIdx * m_elementByteSize;
		return dataPtr;
	}


	template<typename T>
	bool ByteVector::IsScalarType() const
	{
		SEAssert((std::is_same<T, std::uint16_t>::value || std::is_same<T, std::uint32_t>::value),
			"Only uint16_t or uint32_t types are currently supported");

		if constexpr (std::is_same<T, std::uint16_t>::value)
		{
			return m_typeInfoHash == typeid(uint16_t).hash_code();
		}
		else if constexpr (std::is_same<T, std::uint32_t>::value)
		{
			return m_typeInfoHash == typeid(uint32_t).hash_code();
		}
		return false;
	}


	template<typename T>
	T ByteVector::ScalarGetAs(size_t elementIdx) const
	{
		SEAssert((std::is_same<T, std::uint16_t>::value || std::is_same<T, std::uint32_t>::value) &&
			(m_typeInfoHash == typeid(uint16_t).hash_code() ||
				m_typeInfoHash == typeid(uint32_t).hash_code()),
			"Only uint16_t or uint32_t types are currently supported");

		if (m_typeInfoHash == typeid(uint16_t).hash_code())
		{
			SEAssert(elementIdx < std::numeric_limits<uint16_t>::max(), "Element index is OOB");
			return util::CheckedCast<T>(at<uint16_t>(elementIdx));
		}
		else if (m_typeInfoHash == typeid(uint32_t).hash_code())
		{
			SEAssert(elementIdx < std::numeric_limits<uint32_t>::max(), "Element index is OOB");
			return util::CheckedCast<T>(at<uint32_t>(elementIdx));
		}
		else
		{
			SEAssertF("Invalid type");
		}
		return T(0); // This should never happen
	}


	template<typename T>
	void ByteVector::ScalarSetFrom(size_t elementIdx, T val)
	{
		SEAssert((std::is_same<T, std::uint16_t>::value || std::is_same<T, std::uint32_t>::value) &&
			(m_typeInfoHash == typeid(uint16_t).hash_code() ||
				m_typeInfoHash == typeid(uint32_t).hash_code()),
			"Only uint16_t or uint32_t types are currently supported");

		if (m_typeInfoHash == typeid(uint16_t).hash_code())
		{
			SEAssert(elementIdx < std::numeric_limits<uint16_t>::max(), "Element index is OOB");
			at<uint16_t>(elementIdx) = util::CheckedCast<uint16_t>(val);
		}
		else if (m_typeInfoHash == typeid(uint32_t).hash_code())
		{
			SEAssert(elementIdx < std::numeric_limits<uint32_t>::max(), "Element index is OOB");
			at<uint32_t>(elementIdx) = util::CheckedCast<uint32_t>(val);
		}
		else
		{
			SEAssertF("Invalid type");
		}
	}


	inline void ByteVector::Rearrange(std::vector<size_t> const& indexMap)
	{
		std::vector<uint8_t> newData;
		newData.resize(indexMap.size() * m_elementByteSize);

		for (size_t i = 0; i < indexMap.size(); ++i)
		{
			const size_t srcIdx = indexMap[i];

			memcpy(&newData[i * m_elementByteSize], &m_data[srcIdx * m_elementByteSize], m_elementByteSize);
		}

		m_data = std::move(newData);
	}


	inline void ByteVector::CopyElement(ByteVector& dst, size_t dstElemIdx, ByteVector const& src, size_t srcElemIdx)
	{
		SEAssert(dst.m_typeInfoHash == src.m_typeInfoHash && dst.m_elementByteSize == src.m_elementByteSize,
			"Trying to copy elements between Bytevectors with a different underlying type");

		SEAssert(dstElemIdx < dst.size() && srcElemIdx < src.size(), "Element index is OOB");

		void* dstPtr = dst.m_data.data() + (dstElemIdx * dst.m_elementByteSize);
		void const* srcPtr = src.m_data.data() + (srcElemIdx * src.m_elementByteSize);

		memcpy(dstPtr, srcPtr, src.m_elementByteSize);
	}


	inline void ByteVector::EmplaceBackElement(ByteVector& dst, ByteVector const& src, size_t srcElementIdx)
	{
		SEAssert(dst.m_typeInfoHash == src.m_typeInfoHash && dst.m_elementByteSize == src.m_elementByteSize,
			"Trying to copy elements between Bytevectors with a different underlying type");

		SEAssert(srcElementIdx < src.size(), "Element index is OOB");

		const size_t curNumDstBytes = dst.m_data.size();
		dst.m_data.resize(dst.m_data.size() + dst.m_elementByteSize);

		void const* srcBytePtr = src.m_data.data() + (srcElementIdx * src.m_elementByteSize);
		void* dstBytePtr = dst.m_data.data() + curNumDstBytes;

		memcpy(dstBytePtr, srcBytePtr, src.m_elementByteSize);
	}
}