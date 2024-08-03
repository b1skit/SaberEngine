// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IPlatformParams.h"


namespace re
{
	class VertexStream : public virtual core::IHashedDataObject
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};


	public:
		enum Normalize : bool
		{
			False = 0,
			True = 1
		};

		enum class DataType // Of each component in a vertex stream element. Eg. Color/Float4 == Float
		{
			Float,	// 32-bit
			UInt,	// 32-bit
			UShort, // 16-bit
			UByte,	// 8-bit

			// NOTE: If adding more data types, check re::VertexStream::VertexStream() to see if we need to handle
			// additional normalization cases
		};

		enum class StreamType
		{
			Index,
			Vertex
		};

		enum class Lifetime : bool
		{
			SingleFrame,
			Permanent
		};

	public:
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::VertexStream> Create(
			Lifetime, StreamType, uint32_t numComponents, DataType, Normalize, std::vector<T>&& data);

		VertexStream(VertexStream&&) = default;
		VertexStream& operator=(VertexStream&&) = default;

		~VertexStream() { Destroy(); };

		StreamType GetStreamType() const;

		void* GetData();
		void const* GetData() const;

		std::vector<uint8_t> const& GetDataAsVector() const;

		uint32_t GetTotalDataByteSize() const;
		
		uint32_t GetNumComponents() const; // Number of individual components per element. i.e. 1/2/3/4 (only)
		
		uint32_t GetNumElements() const; // How many vertices-worth of attributes do we have?
		uint32_t GetElementByteSize() const; // Total number of bytes for a single element (ie. all components)

		DataType GetDataType() const; // What data type does each individual component have?
		Normalize DoNormalize() const; // Should the data be normalized when it is accessed by the GPU?
		
		Lifetime GetLifetime() const;

		PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }

		void ShowImGuiWindow() const;


	protected:
		void ComputeDataHash() override;


	private:
		void Destroy();
		

	private:
		const Lifetime m_lifetime;
		const StreamType m_streamType;
		uint8_t m_numComponents; 
		uint8_t m_componentByteSize; // Size in bytes of a single component. eg. Float = 4 bytes, Float2 = 8 bytes, etc

		Normalize m_doNormalize;
		DataType m_dataType;

		std::vector<uint8_t> m_data;

		std::unique_ptr<PlatformParams> m_platformParams;


	private: // Use the Create() factory instead
		VertexStream(Lifetime, StreamType, uint32_t numComponents, DataType, Normalize, std::vector<uint8_t>&& data);
		static std::shared_ptr<re::VertexStream> CreateInternal(
			Lifetime, StreamType, uint32_t numComponents, DataType, Normalize, std::vector<uint8_t>&& data);


	private:
		// Share via pointers; no copying allowed
		VertexStream(VertexStream const&) = delete;
		VertexStream& operator=(VertexStream const&) = delete;
	};


	inline VertexStream::StreamType VertexStream::GetStreamType() const
	{
		return m_streamType;
	}


	inline VertexStream::Lifetime VertexStream::GetLifetime() const
	{
		return m_lifetime;
	}


	template<typename T>
	std::shared_ptr<re::VertexStream> VertexStream::Create(
		Lifetime lifetime,
		StreamType type,
		uint32_t numComponents,
		DataType dataType,
		Normalize doNormalize,
		std::vector<T>&& data)
	{
		return CreateInternal(
			lifetime,
			type,
			numComponents,
			dataType,
			doNormalize,
			std::move(*reinterpret_cast<std::vector<uint8_t>*>(&data)) );
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline re::VertexStream::PlatformParams::~PlatformParams() {};
}