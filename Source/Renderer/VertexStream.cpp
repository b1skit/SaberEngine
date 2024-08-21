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


	constexpr char const* TypeToCStr(re::VertexStream::Type type)
	{
		switch (type)
		{
		case re::VertexStream::Type::Position: return "Position";
		case re::VertexStream::Type::Normal: return "Normal";
		case re::VertexStream::Type::Binormal: return "Binormal";
		case re::VertexStream::Type::Tangent: return "Tangent";
		case re::VertexStream::Type::TexCoord: return "TexCoord";
		case re::VertexStream::Type::Color: return "Color";
		case re::VertexStream::Type::BlendIndices: return "BlendIndices";
		case re::VertexStream::Type::BlendWeight: return "BlendWeight";
		case re::VertexStream::Type::Index: return "Index";
		default: return "INVALID_TYPE";
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


	std::shared_ptr<re::VertexStream> VertexStream::Create(CreateParams const& createParams, util::ByteVector&& data)
	{
		std::shared_ptr<re::VertexStream> newVertexStream;
		newVertexStream.reset(new VertexStream(createParams, std::move(data)));

		if (createParams.m_lifetime == Lifetime::SingleFrame)
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


	VertexStream::VertexStream(CreateParams const& createParams, util::ByteVector&& data)
		: m_createParams(createParams)
		, m_data(std::move(data))
		, m_platformParams(nullptr)
	{
		SEAssert(m_createParams.m_type != Type::Type_Count && m_createParams.m_dataType != DataType::DataType_Count,
			"Invalid create params");

		// D3D12 does not support GPU-normalization of 32-bit types. As a hail-mary, we attempt to pre-normalize here
		if (DoNormalize() && 
			createParams.m_dataType == re::VertexStream::DataType::Float ||
			createParams.m_dataType == re::VertexStream::DataType::Float2 ||
			createParams.m_dataType == re::VertexStream::DataType::Float3 ||
			createParams.m_dataType == re::VertexStream::DataType::Float4)
		{
			static const bool s_doNormalize = 
				core::Config::Get()->KeyExists(core::configkeys::k_doCPUVertexStreamNormalizationKey);

			if (s_doNormalize)
			{
				LOG_WARNING("Pre-normalizing vertex stream data as its format is incompatible with GPU-normalization");

				NormalizeData(m_data, createParams.m_dataType);
			}
			else
			{
				LOG_WARNING("Vertex stream is marked for normalization, but its format is incompatible with "
					"GPU-normalization and CPU-side normalization is disabled");
			}

			m_createParams.m_doNormalize = Normalize::False;
		}

		m_platformParams = std::move(platform::VertexStream::CreatePlatformParams(*this, m_createParams.m_type));

		ComputeDataHash();
	}


	void VertexStream::ComputeDataHash()
	{
		AddDataBytesToHash(m_createParams);
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


	uint32_t VertexStream::GetTotalDataByteSize() const
	{
		return util::CheckedCast<uint32_t>(m_data.NumBytes());
	}


	uint32_t VertexStream::GetNumElements() const
	{
		return  util::CheckedCast<uint32_t>(m_data.size()); // i.e. Get the number of vertices
	}


	uint8_t VertexStream::GetElementByteSize() const
	{
		const uint8_t numComponents = DataTypeToNumComponents(m_createParams.m_dataType);
		const uint8_t componentByteSize = DataTypeToComponentByteSize(m_createParams.m_dataType);
		return numComponents * componentByteSize;
	}


	uint8_t VertexStream::GetNumComponents() const
	{
		return DataTypeToNumComponents(m_createParams.m_dataType);
	}


	VertexStream::DataType VertexStream::GetDataType() const
	{
		return m_createParams.m_dataType;
	}


	VertexStream::Normalize VertexStream::DoNormalize() const
	{
		return m_createParams.m_doNormalize;
	}


	void VertexStream::ShowImGuiWindow() const
	{
		ImGui::Text(std::format("Type: {}", TypeToCStr(m_createParams.m_type)).c_str());
		ImGui::Text(std::format("Source type index: {}", m_createParams.m_srcTypeIdx).c_str());

		const bool isMorphTarget = m_createParams.m_isMorphData == VertexStream::IsMorphData::True;
		if (isMorphTarget)
		{
			ImGui::Text(std::format("Morph target index: {}", m_createParams.m_morphTargetIdx).c_str());
		}

		ImGui::Text(std::format("Data type: {}", DataTypeToCStr(m_createParams.m_dataType)).c_str());
		ImGui::Text(std::format("Normalized? {}", (m_createParams.m_doNormalize ? "true" : "false")).c_str());

		ImGui::Text(std::format("Total data byte size: {}", GetTotalDataByteSize()).c_str());
		ImGui::Text(std::format("Number of elements: {}", GetNumElements()).c_str());
		ImGui::Text(std::format("Element byte size: {}", GetElementByteSize()).c_str());
		ImGui::Text(std::format("Number of components: {}", DataTypeToNumComponents(m_createParams.m_dataType)).c_str());
		ImGui::Text(std::format("Component byte size: {}", DataTypeToComponentByteSize(m_createParams.m_dataType)).c_str());
	}
}