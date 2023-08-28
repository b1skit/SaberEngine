// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "MeshPrimitive.h"
#include "VertexStream.h"
#include "VertexStream_Platform.h"


namespace
{
	template <typename T>
	void Normalize(std::vector<T>& data)
	{
		for (T& element : data)
		{
			element = glm::normalize(element);
		}
	}

	void NormalizeData(std::vector<uint8_t>& data, uint32_t numComponents, re::VertexStream::DataType dataType)
	{
		switch (dataType)
		{
		case re::VertexStream::DataType::Float:
		{
			switch (numComponents)
			{
			case 1:
			{
				SEAssertF("Cannot normalize a single component vector");
				return;
			}
			break;
			case 2:
			{
				Normalize(reinterpret_cast<std::vector<glm::vec2>&>(data));
			}
			break;
			case 3:
			{
				Normalize(reinterpret_cast<std::vector<glm::vec3>&>(data));
			}
			break;
			case 4:
			{
				Normalize(reinterpret_cast<std::vector<glm::vec4>&>(data));
			}
			break;
			default:
			{
				SEAssertF("Invalid number of components");
			}
			}
		}
		break;
		case re::VertexStream::DataType::UInt:
		case re::VertexStream::DataType::UByte:
		{
			SEAssertF("Only floating point types can be normalized");
		}
		break;
		default:
		{
			SEAssertF("Invalid data type");
		}
		}
	}


	constexpr char const* DataTypeToCStr(re::VertexStream::DataType dataType)
	{
		switch (dataType)
		{
		case re::VertexStream::DataType::Float: return "Float";
		case re::VertexStream::DataType::UInt: return "UInt";
		case re::VertexStream::DataType::UByte: return "UByte";
		default: SEAssertF("Invalid data type");
		}
		return "INVALID DATA TYPE";
	}
}


namespace re
{
	VertexStream::VertexStream(
		StreamType type, uint32_t numComponents, DataType dataType, Normalize doNormalize, std::vector<uint8_t>&& data)
		: m_numComponents(numComponents)
		, m_dataType(dataType)
		, m_doNormalize(doNormalize)
		, m_platformParams(nullptr)
	{
		SEAssert("Only 1, 2, 3, or 4 components are valid", numComponents >= 1 && numComponents <= 4);

		m_data = std::move(data);

		// D3D12 does not support GPU-normalization of 32 bit types. As a hail-mary, we attempt to pre-normalize here
		if (DoNormalize() && m_dataType == re::VertexStream::DataType::Float)
		{
			LOG_WARNING("Pre-normalizing vertex stream data as its format is incompatible with GPU-normalization");

			NormalizeData(m_data, m_numComponents, m_dataType);

			m_doNormalize = Normalize::False;
		}
		
		switch (dataType)
		{
		case DataType::Float:
		{
			m_componentByteSize = sizeof(float);
		}
		break;
		case DataType::UInt:
		{
			m_componentByteSize = sizeof(uint32_t); // TODO: Support variably-sized indices
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


		m_platformParams = std::move(platform::VertexStream::CreatePlatformParams(*this, type));
	}


	void VertexStream::Destroy()
	{
		platform::VertexStream::Destroy(*this);
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
		// i.e. Get the number of vertices
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


	void VertexStream::ShowImGuiWindow()
	{
		ImGui::Text(std::format("Number of components: {}", m_numComponents).c_str());
		ImGui::Text(std::format("Component byte size: {}", m_componentByteSize).c_str());
		ImGui::Text(std::format("Total data byte size: {}", GetTotalDataByteSize()).c_str());
		ImGui::Text(std::format("Number of elements: {}", GetNumElements()).c_str());
		ImGui::Text(std::format("Element byte size: {}", GetElementByteSize()).c_str());
		ImGui::Text(std::format("Normalized? {}", (m_doNormalize ? "true" : "false")).c_str());
		ImGui::Text(std::format("Data type: {}", DataTypeToCStr(m_dataType)).c_str());
	}
}