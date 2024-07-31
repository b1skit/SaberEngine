// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Core/Config.h"
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "SceneManager.h"
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

	template<>
	void Normalize(std::vector<glm::vec4>& data)
	{
		LOG_WARNING("Vertex stream is requesting to normalize a 4-component vector. Assuming it is a "
			"3-component XYZ vector, with a packed value in .w");

		for (glm::vec4& element : data)
		{
			element.xyz = glm::normalize(static_cast<glm::vec3>(element.xyz));
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
		case re::VertexStream::DataType::UShort:
		case re::VertexStream::DataType::UByte:
		{
			SEAssertF("Only floating point types can (currently) be normalized");
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
		case re::VertexStream::DataType::UShort: return "UShort";
		case re::VertexStream::DataType::UByte: return "UByte";
		default: SEAssertF("Invalid data type");
		}
		return "INVALID DATA TYPE";
	}
}


namespace re
{
	std::shared_ptr<re::VertexStream> VertexStream::CreateInternal(
		Lifetime lifetime, 
		StreamType type, 
		uint32_t numComponents, 
		DataType dataType, 
		Normalize doNormalize, 
		std::vector<uint8_t>&& data)
	{
		std::shared_ptr<re::VertexStream> newVertexStream;
		newVertexStream.reset(new VertexStream(
			lifetime,
			type,
			numComponents,
			dataType,
			doNormalize,
			std::move(data)));

		if (lifetime == Lifetime::SingleFrame)
		{
			re::RenderManager::Get()->RegisterSingleFrameResource(newVertexStream);
			re::RenderManager::Get()->RegisterForCreate(newVertexStream);
		}
		else
		{
			bool duplicateExists = re::RenderManager::GetSceneData()->AddUniqueVertexStream(newVertexStream);
			if (!duplicateExists)
			{
				re::RenderManager::Get()->RegisterForCreate(newVertexStream);
			}
		}
		
		return newVertexStream;
	}


	VertexStream::VertexStream(
		Lifetime lifetime, 
		StreamType type, 
		uint32_t numComponents, 
		DataType dataType, 
		Normalize doNormalize, 
		std::vector<uint8_t>&& data)
		: m_lifetime(lifetime)
		, m_streamType(type)
		, m_numComponents(numComponents)
		, m_dataType(dataType)
		, m_doNormalize(doNormalize)
		, m_platformParams(nullptr)
	{
		SEAssert(numComponents >= 1 && numComponents <= 4, "Only 1, 2, 3, or 4 components are valid");

		m_data = std::move(data);

		// D3D12 does not support GPU-normalization of 32-bit types. As a hail-mary, we attempt to pre-normalize here
		if (DoNormalize() && m_dataType == re::VertexStream::DataType::Float)
		{
			static const bool s_doNormalize = 
				core::Config::Get()->KeyExists(core::configkeys::k_doCPUVertexStreamNormalizationKey);

			if (s_doNormalize)
			{
				LOG_WARNING("Pre-normalizing vertex stream data as its format is incompatible with GPU-normalization");

				NormalizeData(m_data, m_numComponents, m_dataType);
			}
			else
			{
				LOG_WARNING("Vertex stream is marked for normalization, but its format is incompatible with "
					"GPU-normalization and CPU-side normalization is disabled");
			}

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
		case DataType::UShort:
		{
			m_componentByteSize = sizeof(unsigned short);
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
		SEAssert(m_data.size() % ((static_cast<size_t>(m_numComponents) * m_componentByteSize)) == 0,
			"Data and description don't match");


		m_platformParams = std::move(platform::VertexStream::CreatePlatformParams(*this, type));

		ComputeDataHash();
	}


	void VertexStream::ComputeDataHash()
	{
		AddDataBytesToHash(m_streamType);
		AddDataBytesToHash(m_numComponents);
		AddDataBytesToHash(m_componentByteSize);
		AddDataBytesToHash(m_doNormalize);
		AddDataBytesToHash(m_dataType);

		AddDataBytesToHash(GetData(), GetTotalDataByteSize());
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
		SEAssert(m_numComponents > 0 && m_componentByteSize > 0, "Invalid denominator");
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


	void VertexStream::ShowImGuiWindow() const
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