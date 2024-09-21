// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "VertexStream.h"
#include "VertexStream_Platform.h"

#include "Core/Assert.h"
#include "Core/Config.h"

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

	void NormalizeData(util::ByteVector& data, re::DataType dataType)
	{
		const uint8_t numComponents = re::DataTypeToNumComponents(dataType);

		switch (dataType)
		{
		case re::DataType::Float:
		case re::DataType::Float2:
		case re::DataType::Float3:
		case re::DataType::Float4:
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
	std::shared_ptr<re::VertexStream> VertexStream::Create(CreateParams const& createParams, util::ByteVector&& data)
	{
		std::shared_ptr<re::VertexStream> newVertexStream;
		newVertexStream.reset(new VertexStream(createParams, std::move(data)));

		if (createParams.m_lifetime == re::Lifetime::SingleFrame)
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
		, m_platformParams(nullptr)
	{
		SEAssert(m_createParams.m_type != Type::Type_Count && m_createParams.m_dataType != DataType::DataType_Count,
			"Invalid create params");

		SEAssert(m_createParams.m_type != Type::Index || 
			(m_createParams.m_dataType == DataType::UShort && data.IsScalarType<uint16_t>()) ||
			(m_createParams.m_dataType == DataType::UInt && data.IsScalarType<uint32_t>()),
			"Invalid index data");

		bool isNormalized = false;

		// D3D12 does not support GPU-normalization of 32-bit types. As a hail-mary, we attempt to pre-normalize here
		if (DoNormalize() && 
			createParams.m_dataType == re::DataType::Float ||
			createParams.m_dataType == re::DataType::Float2 ||
			createParams.m_dataType == re::DataType::Float3 ||
			createParams.m_dataType == re::DataType::Float4)
		{
			static const bool s_doNormalize = 
				core::Config::Get()->KeyExists(core::configkeys::k_doCPUVertexStreamNormalizationKey);

			if (s_doNormalize)
			{
				LOG_WARNING("Pre-normalizing vertex stream data as its format is incompatible with GPU-normalization");

				NormalizeData(data, createParams.m_dataType);
			}
			else
			{
				LOG_WARNING("Vertex stream is marked for normalization, but its format is incompatible with "
					"GPU-normalization and CPU-side normalization is disabled");
			}

			m_createParams.m_doNormalize = Normalize::False;
			isNormalized = true;
		}

		m_platformParams = std::move(platform::VertexStream::CreatePlatformParams(*this, m_createParams.m_type));

		// Create the vertex/index Buffer object that will back our vertex stream:
		std::string const& bufferName = std::format("VertexStream_{}_{}", TypeToCStr(GetType()), GetDataHash());
		
		m_streamBuffer = re::Buffer::Create(
			bufferName,
			data.data().data(),
			util::CheckedCast<uint32_t>(data.GetTotalNumBytes()),
			re::Buffer::BufferParams{
				.m_allocationType = re::Buffer::AllocationType::Immutable,
				.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Default,
				.m_usageMask = re::Buffer::Usage::GPURead,
				.m_type = m_createParams.m_type == re::VertexStream::Type::Index ? 
					re::Buffer::Type::Index : re::Buffer::Type::Vertex,
				.m_arraySize = 1,
			.m_typeParams = { .m_vertexStream = {
				.m_dataType = m_createParams.m_dataType,
				.m_isNormalized = isNormalized,
				.m_stride = GetElementByteSize()}}
			});

		ComputeDataHash();
	}


	void VertexStream::ComputeDataHash()
	{
		AddDataBytesToHash(m_createParams);
		AddDataBytesToHash(m_streamBuffer->GetData(), m_streamBuffer->GetTotalBytes());
	}


	void VertexStream::Destroy()
	{
		platform::VertexStream::Destroy(*this);
	}


	uint32_t VertexStream::GetTotalDataByteSize() const
	{
		return m_streamBuffer->GetTotalBytes();
	}


	uint32_t VertexStream::GetNumElements() const
	{
		return m_streamBuffer->GetTotalBytes() / GetElementByteSize();
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


	re::DataType VertexStream::GetDataType() const
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

		const bool isMorphTarget = m_createParams.m_isMorphData == VertexStream::IsMorphData::True;
		ImGui::Text(std::format("Is morph target data? {}", isMorphTarget ? "true" : "false").c_str());

		ImGui::Text(std::format("Data type: {}", re::DataTypeToCStr(m_createParams.m_dataType)).c_str());
		ImGui::Text(std::format("Normalized? {}", (m_createParams.m_doNormalize ? "true" : "false")).c_str());

		ImGui::Text(std::format("Total data byte size: {}", GetTotalDataByteSize()).c_str());
		ImGui::Text(std::format("Number of elements: {}", GetNumElements()).c_str());
		ImGui::Text(std::format("Element byte size: {}", GetElementByteSize()).c_str());
		ImGui::Text(std::format("Number of components: {}", DataTypeToNumComponents(m_createParams.m_dataType)).c_str());
		ImGui::Text(std::format("Component byte size: {}", DataTypeToComponentByteSize(m_createParams.m_dataType)).c_str());
	}
}