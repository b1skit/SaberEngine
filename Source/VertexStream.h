// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"


namespace re
{
	class VertexStream
	{
	public:
		struct PlatformParams : public re::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};


	public:
		enum Normalize : bool
		{
			False = 0,
			True = 1
		};

		enum class DataType // Of each component in a vertex. Eg. Color/Float4 == Float
		{
			Float,	// 32-bit
			UInt,	// 32-bit
			UByte,	// 8-bit

			// NOTE: If adding more data types, check re::VertexStream::VertexStream() to see if we need to handle
			// additional normalization cases

			DataType_Count
		};

		enum class StreamType
		{
			Index,
			Vertex,
			StreamType_Count
		};


	public:
		VertexStream(
			StreamType type, uint32_t numComponents, DataType dataType, Normalize doNormalize, std::vector<uint8_t>&& data);
		VertexStream(VertexStream&&) = default;
		VertexStream& operator=(VertexStream&&) = default;
		~VertexStream() { Destroy(); };

		void* GetData();
		void const* GetData() const;

		std::vector<uint8_t> const& GetDataAsVector() const;

		uint32_t GetTotalDataByteSize() const;
		
		uint32_t GetNumComponents() const; // Number of individual components per element. i.e. 1/2/3/4 (only)
		
		uint32_t GetNumElements() const; // How many vertices-worth of attributes do we have?
		uint32_t GetElementByteSize() const; // Total number of bytes for a single element (ie. all components)

		DataType GetDataType() const; // What data type does each individual component have?
		Normalize DoNormalize() const; // Should the data be normalized when it is accessed by the GPU?
		
		PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }

		void ShowImGuiWindow();


	private:
		void Destroy();

	private:
		uint8_t m_numComponents; 
		uint8_t m_componentByteSize; // Size in bytes of a single component. eg. Float = 4 bytes, Float2 = 8 bytes, etc

		Normalize m_doNormalize;
		DataType m_dataType;

		std::vector<uint8_t> m_data;

		std::unique_ptr<PlatformParams> m_platformParams;


	private:
		// Share via pointers; no copying allowed
		VertexStream(VertexStream const&) = delete;
		VertexStream& operator=(VertexStream const&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline re::VertexStream::PlatformParams::~PlatformParams() {};
}