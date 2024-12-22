// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "BufferView.h"
#include "RenderManager.h"
#include "VertexStream.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/InvPtr.h"

#include "Core/Interfaces/ILoadContext.h"

#include "Core/Util/DataHash.h"


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


	constexpr char const* TypeToCStr(gr::VertexStream::Type type)
	{
		switch (type)
		{
		case gr::VertexStream::Type::Position: return "Position";
		case gr::VertexStream::Type::Normal: return "Normal";
		case gr::VertexStream::Type::Binormal: return "Binormal";
		case gr::VertexStream::Type::Tangent: return "Tangent";
		case gr::VertexStream::Type::TexCoord: return "TexCoord";
		case gr::VertexStream::Type::Color: return "Color";
		case gr::VertexStream::Type::BlendIndices: return "BlendIndices";
		case gr::VertexStream::Type::BlendWeight: return "BlendWeight";
		case gr::VertexStream::Type::Index: return "Index";
		default: return "INVALID_TYPE";
		}
	}


	util::DataHash ComputeVertexStreamDataHash(
		gr::VertexStream::StreamDesc const& streamDesc, void const* data, size_t numBytes)
	{
		util::DataHash result = util::HashDataBytes(data, numBytes);
		util::AddDataBytesToHash(result, streamDesc);
		
		return result;
	}
}


namespace gr
{
	core::InvPtr<gr::VertexStream> VertexStream::Create(
		StreamDesc const& streamDesc,
		util::ByteVector&& data,
		re::Buffer::UsageMask extraUsageBits /*= 0*/)
	{		
		// Vertex streams use a data hash as their ID (to allow sharing/reuse). Thus, we must compute it before we can
		// make a decision about whether to actually create the stream or not
		const util::DataHash streamDataHash =
			ComputeVertexStreamDataHash(streamDesc, data.data().data(), data.GetTotalNumBytes());

		core::Inventory* inventory = re::RenderManager::Get()->GetInventory();
		if (inventory->HasLoaded<gr::VertexStream>(streamDataHash))
		{
			return inventory->Get<gr::VertexStream>(streamDataHash);
		}


		struct VertexStreamLoadContext : core::ILoadContext<gr::VertexStream>
		{
			std::unique_ptr<gr::VertexStream> Load(core::InvPtr<gr::VertexStream>) override
			{
				return std::unique_ptr<gr::VertexStream>(
					new VertexStream(m_streamDesc, std::move(m_data), m_dataHash, m_extraUsageBits));
			}

			util::DataHash m_dataHash;

			StreamDesc m_streamDesc;
			util::ByteVector m_data;
			re::Buffer::UsageMask m_extraUsageBits;
		};
		std::shared_ptr<VertexStreamLoadContext> vertexStreamLoadContext = std::make_shared<VertexStreamLoadContext>();

		vertexStreamLoadContext->m_dataHash = streamDataHash;
		vertexStreamLoadContext->m_streamDesc = streamDesc;
		vertexStreamLoadContext->m_data = std::move(data);
		vertexStreamLoadContext->m_extraUsageBits = extraUsageBits;

		return inventory->Get(
			streamDataHash,
			static_pointer_cast<core::ILoadContext<gr::VertexStream>>(vertexStreamLoadContext));
	}


	core::InvPtr<gr::VertexStream> VertexStream::Create(CreateParams&& createParams)
	{
		return Create(
			createParams.m_streamDesc, std::move(*createParams.m_streamData.get()), createParams.m_extraUsageBits);
	}


	VertexStream::VertexStream(
		StreamDesc const& streamDesc,
		util::ByteVector&& data,
		util::DataHash dataHash,
		re::Buffer::UsageMask extraUsageBits)
		: m_streamDesc(streamDesc)
	{
		SEAssert(m_streamDesc.m_type != Type::Type_Count && m_streamDesc.m_dataType != re::DataType::DataType_Count,
			"Invalid create params");

		SEAssert(m_streamDesc.m_type != Type::Index || 
			(m_streamDesc.m_dataType == re::DataType::UShort && data.IsScalarType<uint16_t>()) ||
			(m_streamDesc.m_dataType == re::DataType::UInt && data.IsScalarType<uint32_t>()),
			"Invalid index data");

		// D3D12 does not support GPU-normalization of 32-bit types. As a hail-mary, we attempt to pre-normalize here
		if (DoNormalize() && 
			(streamDesc.m_dataType == re::DataType::Float ||
				streamDesc.m_dataType == re::DataType::Float2 ||
				streamDesc.m_dataType == re::DataType::Float3 ||
				streamDesc.m_dataType == re::DataType::Float4))
		{
			static const bool s_doNormalize = 
				core::Config::Get()->KeyExists(core::configkeys::k_doCPUVertexStreamNormalizationKey);

			if (s_doNormalize)
			{
				LOG_WARNING("Pre-normalizing vertex stream data as its format is incompatible with GPU-normalization");

				NormalizeData(data, streamDesc.m_dataType);
			}
			else
			{
				LOG_WARNING("Vertex stream is marked for normalization, but its format is incompatible with "
					"GPU-normalization and CPU-side normalization is disabled");
			}

			m_streamDesc.m_doNormalize = Normalize::False;
		}

		// Force-set the pre-computed data hash
		SetDataHash(dataHash);


		// Create the vertex/index Buffer object that will back our vertex stream:
		std::string const& bufferName =
			std::format("VertexStream_{}_{}", TypeToCStr(m_streamDesc.m_type), GetDataHash());

		const re::Buffer::MemoryPoolPreference bufMemPoolPref =
			streamDesc.m_lifetime == re::Lifetime::SingleFrame ? re::Buffer::UploadHeap : re::Buffer::DefaultHeap;

		const re::Buffer::UsageMask bufferUsage =
			(streamDesc.m_type == Type::Index ? re::Buffer::IndexStream : re::Buffer::VertexStream) | extraUsageBits;

		re::Buffer::AccessMask bufAccessMask = re::Buffer::GPURead;
		if (bufMemPoolPref == re::Buffer::UploadHeap)
		{
			bufAccessMask |= re::Buffer::CPUWrite;
		}

		m_streamBuffer = re::Buffer::Create(
			bufferName,
			data.data().data(),
			util::CheckedCast<uint32_t>(data.GetTotalNumBytes()),
			re::Buffer::BufferParams{
				.m_lifetime = streamDesc.m_lifetime,
				.m_stagingPool = re::Buffer::StagingPool::Temporary,
				.m_memPoolPreference = bufMemPoolPref,
				.m_accessMask = bufAccessMask,
				.m_usageMask = bufferUsage,
				.m_arraySize = 1, });
	}


	void VertexStream::ComputeDataHash()
	{
		AddDataBytesToHash(m_streamDesc);
	}


	VertexStream::~VertexStream()
	{
		SEAssert(m_streamBuffer == nullptr,
			"Vertex stream DTOR called, but m_streamBuffer is not null. Was Destroy() called?");
	}


	void VertexStream::Destroy()
	{
		m_streamBuffer = nullptr;
	}


	uint32_t VertexStream::GetTotalDataByteSize() const
	{
		return m_streamBuffer->GetTotalBytes();
	}


	uint32_t VertexStream::GetNumElements() const
	{
		return m_streamBuffer->GetTotalBytes() / DataTypeToByteStride(m_streamDesc.m_dataType);
	}


	re::DataType VertexStream::GetDataType() const
	{
		return m_streamDesc.m_dataType;
	}


	bool VertexStream::DoNormalize() const
	{
		return static_cast<bool>(m_streamDesc.m_doNormalize);
	}


	void VertexStream::ShowImGuiWindow() const
	{
		ImGui::Text(std::format("Type: {}", TypeToCStr(m_streamDesc.m_type)).c_str());

		ImGui::Text(std::format("Data type: {}", re::DataTypeToCStr(m_streamDesc.m_dataType)).c_str());
		ImGui::Text(std::format("Normalized? {}", (m_streamDesc.m_doNormalize ? "true" : "false")).c_str());

		ImGui::Text(std::format("Total data byte size: {}", GetTotalDataByteSize()).c_str());
		ImGui::Text(std::format("Number of elements: {}", GetNumElements()).c_str());
		ImGui::Text(std::format("Number of components: {}", DataTypeToNumComponents(m_streamDesc.m_dataType)).c_str());
		ImGui::Text(std::format("Component byte size: {}", DataTypeToComponentByteSize(m_streamDesc.m_dataType)).c_str());
	}
}