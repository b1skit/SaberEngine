// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Core/Config.h"
#include "MeshPrimitive.h"
#include "RenderManager.h"
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

	void NormalizeData(util::ByteVector& data, re::VertexStream::DataType dataType)
	{
		const uint8_t numComponents = re::VertexStream::DataTypeToNumComponents(dataType);

		switch (dataType)
		{
		case re::VertexStream::DataType::Float:
		case re::VertexStream::DataType::Float2:
		case re::VertexStream::DataType::Float3:
		case re::VertexStream::DataType::Float4:
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
		default: SEAssertF("Unexpected data type for normalization");
		}
	}
}


namespace re
{
	constexpr uint8_t VertexStream::DataTypeToNumComponents(DataType dataType)
	{
		switch (dataType)
		{
		case VertexStream::DataType::Float:
		case VertexStream::DataType::UInt:
		case VertexStream::DataType::UShort:
		case VertexStream::DataType::UByte:
			return 1;
		case VertexStream::DataType::Float2:
		case VertexStream::DataType::UInt2:
		case VertexStream::DataType::UShort2:
		case VertexStream::DataType::UByte2:
			return 2;
		case VertexStream::DataType::Float3:
		case VertexStream::DataType::UInt3:
			return 3;
		case VertexStream::DataType::Float4:
		case VertexStream::DataType::UInt4:
		case VertexStream::DataType::UShort4:
		case VertexStream::DataType::UByte4:
			return 4;		
		default: return std::numeric_limits<uint8_t>::max(); // Error
		}
	}


	constexpr uint8_t VertexStream::DataTypeToComponentByteSize(DataType dataType)
	{
		switch (dataType)
		{
		case VertexStream::DataType::Float:		// 32-bit
		case VertexStream::DataType::Float2:
		case VertexStream::DataType::Float3:
		case VertexStream::DataType::Float4:

		case VertexStream::DataType::Int:		// 32-bit
		case VertexStream::DataType::Int2:
		case VertexStream::DataType::Int3:
		case VertexStream::DataType::Int4:

		case VertexStream::DataType::UInt:		// 32-bit
		case VertexStream::DataType::UInt2:
		case VertexStream::DataType::UInt3:
		case VertexStream::DataType::UInt4:
			return 4;

		case VertexStream::DataType::Short:	// 16-bit
		case VertexStream::DataType::Short2:
		case VertexStream::DataType::Short4:

		case VertexStream::DataType::UShort:	// 16-bit
		case VertexStream::DataType::UShort2:
		case VertexStream::DataType::UShort4:
			return 2;

		case VertexStream::DataType::Byte:		// 8-bit
		case VertexStream::DataType::Byte2:
		case VertexStream::DataType::Byte4:

		case VertexStream::DataType::UByte:		// 8-bit
		case VertexStream::DataType::UByte2:
		case VertexStream::DataType::UByte4:
			return 1;
		default: return 0; // Error
		}
	}


	std::shared_ptr<re::VertexStream> VertexStream::CreateInternal(
		Lifetime lifetime, 
		Type type,
		uint8_t srcIdx,
		DataType dataType, 
		Normalize doNormalize, 
		util::ByteVector&& data)
	{
		std::shared_ptr<re::VertexStream> newVertexStream;
		newVertexStream.reset(new VertexStream(
			lifetime,
			type,
			srcIdx,
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
		Type type, 
		uint8_t srcIdx,
		DataType dataType, 
		Normalize doNormalize, 
		util::ByteVector&& data)
		: m_lifetime(lifetime)
		, m_streamType(type)
		, m_sourceChannelSemanticIdx(srcIdx)
		, m_dataType(dataType)
		, m_data(std::move(data))
		, m_doNormalize(doNormalize)
		, m_platformParams(nullptr)
	{
		// D3D12 does not support GPU-normalization of 32-bit types. As a hail-mary, we attempt to pre-normalize here
		if (DoNormalize() && 
			m_dataType == re::VertexStream::DataType::Float ||
			m_dataType == re::VertexStream::DataType::Float2 ||
			m_dataType == re::VertexStream::DataType::Float3 ||
			m_dataType == re::VertexStream::DataType::Float4)
		{
			static const bool s_doNormalize = 
				core::Config::Get()->KeyExists(core::configkeys::k_doCPUVertexStreamNormalizationKey);

			if (s_doNormalize)
			{
				LOG_WARNING("Pre-normalizing vertex stream data as its format is incompatible with GPU-normalization");

				NormalizeData(m_data, m_dataType);
			}
			else
			{
				LOG_WARNING("Vertex stream is marked for normalization, but its format is incompatible with "
					"GPU-normalization and CPU-side normalization is disabled");
			}

			m_doNormalize = Normalize::False;
		}

		m_platformParams = std::move(platform::VertexStream::CreatePlatformParams(*this, type));

		ComputeDataHash();
	}


	void VertexStream::ComputeDataHash()
	{
		AddDataBytesToHash(m_lifetime);
		AddDataBytesToHash(m_streamType);
		AddDataBytesToHash(m_sourceChannelSemanticIdx);

		AddDataBytesToHash(m_doNormalize);
		AddDataBytesToHash(m_dataType);

		AddDataBytesToHash(GetData(), GetTotalDataByteSize());
	}


	void VertexStream::Destroy()
	{
		platform::VertexStream::Destroy(*this);
	}


	void const* VertexStream::GetData() const
	{
		if (m_data.empty())
		{
			return nullptr;
		}
		return m_data.data().data();
	}


	util::ByteVector const& VertexStream::GetDataByteVector() const
	{
		return m_data;
	}


	uint32_t VertexStream::GetTotalDataByteSize() const
	{
		return util::CheckedCast<uint32_t>(m_data.NumBytes());
	}


	uint32_t VertexStream::GetNumElements() const
	{
		// i.e. Get the number of vertices
		const uint8_t numComponents = DataTypeToNumComponents(m_dataType);
		const uint8_t componentByteSize = DataTypeToComponentByteSize(m_dataType);
		return util::CheckedCast<uint32_t>(m_data.NumBytes()) / (numComponents * componentByteSize);
	}


	uint8_t VertexStream::GetElementByteSize() const
	{
		const uint8_t numComponents = DataTypeToNumComponents(m_dataType);
		const uint8_t componentByteSize = DataTypeToComponentByteSize(m_dataType);
		return numComponents * componentByteSize;
	}


	uint8_t VertexStream::GetNumComponents() const
	{
		return DataTypeToNumComponents(m_dataType);
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
		ImGui::Text(std::format("Number of components: {}", DataTypeToNumComponents(m_dataType)).c_str());
		ImGui::Text(std::format("Component byte size: {}", DataTypeToComponentByteSize(m_dataType)).c_str());
		ImGui::Text(std::format("Total data byte size: {}", GetTotalDataByteSize()).c_str());
		ImGui::Text(std::format("Number of elements: {}", GetNumElements()).c_str());
		ImGui::Text(std::format("Element byte size: {}", GetElementByteSize()).c_str());
		ImGui::Text(std::format("Normalized? {}", (m_doNormalize ? "true" : "false")).c_str());
		ImGui::Text(std::format("Data type: {}", DataTypeToCStr(m_dataType)).c_str());
	}
}