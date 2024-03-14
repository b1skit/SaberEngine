// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "NamedObject.h"


namespace re
{
	/*******************************************************************************************************************
	* Buffers have 2 modification/access types:
	* 1) Mutable:		Can be modified, and are rebuffered when modification is detected
	* 2) Immutable:		Buffered once at creation, and cannot be modified
	*
	* Buffers have 2 lifetime scopes:
	* 1) Permanent:		Allocated once, and held for the lifetime of the program
	* 2) Single frame:	Allocated and destroyed within a single frame
	*					-> Single frame buffers are immutable once they are committed
	*
	* The union of these gives us Permanent Mutable, Permanent Immutable, & SingleFrame Immutable Buffer types
	*******************************************************************************************************************/

	class Buffer : public virtual en::NamedObject
	{
	public:
		enum Type : uint8_t
		{
			Mutable,		// Permanent, can be updated
			Immutable,		// Permanent, cannot be updated
			SingleFrame,	// Single frame, immutable once committed

			Type_Count
		};

		enum DataType : uint8_t
		{
			SingleElement,
			Array,

			DataType_Count
		};


	public:
		struct PlatformParams : public re::IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCommitted = false; // Has an initial data commitment been made?
			bool m_isCreated = false; // Has the buffer been created at the API level?

			DataType m_dataType = DataType::DataType_Count;
			uint32_t m_numElements = 0;
		};


	public: // Buffer factories:

		// Create a buffer for a single data object (eg. stage buffer)
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> Create(
			std::string const& bufferName, T const& data, Type bufferType);

		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> CreateUncommitted(
			std::string const& bufferName, Type bufferType);

		// Create a buffer for an array of several objects of the same type (eg. instanced mesh matrices)
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> CreateArray(
			std::string const& bufferName, T const* dataArray, uint32_t numElements, Type bufferType);

		// Create a buffer, but defer the initial commit
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> CreateUncommittedArray(
			std::string const& bufferName, uint32_t numElements, Type bufferType);


	public:
		Buffer(Buffer&&) = default;
		Buffer& operator=(Buffer&&) = default;
		~Buffer();

		void Destroy();


	public:
		template <typename T>
		void Commit(T const& data); // Commit *updated* data
		
		template <typename T>
		void Commit(T const* data, uint32_t baseIdx, uint32_t numElements); // Recommit mutable array data (only)
	
		void GetDataAndSize(void const*& out_data, uint32_t& out_numBytes) const;
		uint32_t GetSize() const;
		uint32_t GetStride() const;
		inline Type GetType() const { return m_type; }

		uint32_t GetNumElements() const; // Instanced buffers: How many instances of data does the buffer hold?

		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }

	private:		
		uint64_t m_typeIDHash; // Hash of the typeid(T) at Create: Used to verify committed data types don't change

		const Type m_type;

		std::unique_ptr<PlatformParams> m_platformParams;
		

	private:
		// Use the factory Create() method instead
		Buffer(size_t typeIDHashCode, std::string const& bufferName, Type, DataType, uint32_t numElements); 

		static void Register(
			std::shared_ptr<re::Buffer> newBuffer, uint32_t numBytes, uint64_t typeIDHash);

		static void RegisterAndCommit(
			std::shared_ptr<re::Buffer> newBuffer, void const* data, uint32_t numBytes, uint64_t typeIDHash);
		
		void CommitInternal(void const* data, uint64_t typeIDHash);

		void CommitInternal(void const* data, uint32_t baseOffset, uint32_t numBytes, uint64_t typeIDHash); // Partial


	private:
		Buffer() = delete;
		Buffer(Buffer const&) = delete;
		Buffer& operator=(Buffer const&) = delete;
	};


	// Create a buffer for a single data object (eg. stage buffer)
	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::Create(std::string const& bufferName, T const& data, Type bufferType)
	{
		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new re::Buffer(typeid(T).hash_code(), bufferName, bufferType, DataType::SingleElement, 1));

		RegisterAndCommit(newBuffer, &data, sizeof(T), typeid(T).hash_code());

		return newBuffer;
	}


	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::CreateUncommitted(std::string const& bufferName, Type bufferType)
	{
		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new re::Buffer(typeid(T).hash_code(), bufferName, bufferType, DataType::SingleElement, 1));

		Register(newBuffer, sizeof(T), typeid(T).hash_code());

		return newBuffer;
	}


	// Create a buffer for an array of several objects of the same type (eg. instanced mesh matrices)
	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::CreateArray(
		std::string const& bufferName, T const* dataArray, uint32_t numElements, Type bufferType)
	{
		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(
			typeid(T).hash_code(), 
			bufferName, 
			bufferType, 
			DataType::Array, 
			numElements));

		RegisterAndCommit(newBuffer, dataArray, sizeof(T) * numElements, typeid(T).hash_code());

		return newBuffer;
	}


	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::CreateUncommittedArray(
		std::string const& bufferName, uint32_t numElements, Type bufferType)
	{
		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(
			typeid(T).hash_code(),
			bufferName,
			bufferType,
			DataType::Array,
			numElements));

		Register(newBuffer, sizeof(T) * numElements, typeid(T).hash_code());
		
		return newBuffer;
	}


	template <typename T>
	void Buffer::Commit(T const& data) // Commit *updated* data
	{
		CommitInternal(&data, typeid(T).hash_code());
	}


	template <typename T>
	void Buffer::Commit(T const* data, uint32_t baseIdx, uint32_t numElements)
	{
		const uint32_t dstBaseByteOffset = baseIdx * sizeof(T);
		const uint32_t numBytes = numElements * sizeof(T);

		CommitInternal(data, dstBaseByteOffset, numBytes, typeid(T).hash_code());
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Buffer::PlatformParams::~PlatformParams() {};
}