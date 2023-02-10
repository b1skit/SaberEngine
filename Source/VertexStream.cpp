// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "VertexStream.h"
#include "VertexStream_Platform.h"


namespace re
{
	VertexStream::VertexStream(
		uint32_t numComponents, DataType dataType, Normalize doNormalize, std::vector<uint8_t>&& data)
		: m_numComponents(numComponents)
		, m_dataType(dataType)
		, m_doNormalize(doNormalize)
		, m_platformParams(nullptr)
	{
		SEAssert("Only 1, 2, 3, or 4 components are valid", numComponents >= 1 && numComponents <= 4);

		m_platformParams = std::move(platform::VertexStream::CreatePlatformParams());
		
		switch (dataType)
		{
		case DataType::Float:
		{
			m_componentByteSize = sizeof(float);
		}
		break;
		case DataType::UInt:
		{
			m_componentByteSize = sizeof(uint32_t);
		}
		break;
		case DataType::UByte:
		{
			m_componentByteSize = sizeof(uint8_t);
		}
		break;
		default:
			SEAssertF("Invalid data type");
		}
		SEAssert("Data and description don't match",
			data.size() % ((static_cast<size_t>(numComponents) * m_componentByteSize)) == 0);

		m_data = std::move(data);
	}


	void* VertexStream::GetData()
	{
		if (m_data.empty())
		{
			return nullptr;
		}
		return &m_data[0];
	}


	void const* VertexStream::GetData() const
	{
		if (m_data.empty())
		{
			return nullptr;
		}
		return &m_data[0];
	}


	std::vector<uint8_t> const& VertexStream::GetDataAsVector() const
	{
		return m_data;
	}


	uint32_t VertexStream::GetTotalDataByteSize() const
	{
		return static_cast<uint32_t>(m_data.size());
	}


	uint32_t VertexStream::GetNumElements() const
	{
		SEAssert("Invalid denominator", m_numComponents > 0 && m_componentByteSize > 0);
		return static_cast<uint32_t>(m_data.size() / (m_numComponents * m_componentByteSize));
	}


	uint32_t VertexStream::GetElementByteSize() const
	{
		return m_numComponents * m_componentByteSize;
	}


	uint32_t VertexStream::GetNumComponents() const
	{
		return m_numComponents;
	}


	VertexStream::DataType VertexStream::GetDataType() const
	{
		return m_dataType;
	}


	VertexStream::Normalize VertexStream::DoNormalize() const
	{
		return m_doNormalize;
	}
}