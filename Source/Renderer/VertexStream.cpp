// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "BufferInput.h"
#include "RenderManager.h"
#include "VertexStream.h"

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
}


namespace gr
{
	std::shared_ptr<gr::VertexStream> VertexStream::Create(
		StreamDesc const& createParams, util::ByteVector&& data, bool queueBufferCreate /*= true*/)
	{
		// NOTE: Currently we need to defer creating the VertexStream's backing re::Buffer from the front end thread 
		// with the ugliness here: If queueBufferCreate == true, we'll enqueue a render command to create the buffer
		// on the render thread. This will go away once we have a proper async loading system
		bool isNormalized = false; // Temporary: required for buffer thread fix
		bool didCreate = false; // Temporary: required for buffer thread fix

		
		// Currently, we pass the data to the ctor by reference so it can be normalized. We move it to the buffer later
		std::shared_ptr<gr::VertexStream> newVertexStream;
		newVertexStream.reset(new VertexStream(createParams, data, isNormalized));
		
		if (createParams.m_lifetime == re::Lifetime::SingleFrame)
		{
			didCreate = true;
		}
		else
		{
			bool duplicateExists = re::RenderManager::GetSceneData()->AddUniqueVertexStream(newVertexStream);
			if (!duplicateExists)
			{
				didCreate = true;
			}
		}

		// Temporary: required for buffer thread fix
		if (didCreate) 
		{
			// Create the vertex/index Buffer object that will back our vertex stream:
			std::string const& bufferName = std::format(
				"VertexStream_{}_{}", 
				TypeToCStr(newVertexStream->GetType()), 
				newVertexStream->GetDataHash());
			
			const re::Buffer::BufferParams bufferParams{
				.m_stagingPool = re::Buffer::StagingPool::Temporary,
				.m_memPoolPreference = re::Buffer::DefaultHeap,
				.m_accessMask = re::Buffer::GPURead,
				.m_usageMask = createParams.m_type == Type::Index ? re::Buffer::IndexStream : re::Buffer::VertexStream,
				.m_arraySize = 1,
			};
						
			if (queueBufferCreate)
			{
				class CreateBufferDeferred
				{
				public:
					CreateBufferDeferred(
						gr::VertexStream* vertexStream,
						std::string bufferName,
						util::ByteVector&& data,
						re::Buffer::BufferParams bufferParams)
						: m_vertexStream(vertexStream)
						, m_bufferName(bufferName)
						, m_data(std::make_unique<util::ByteVector>(std::move(data)))
						, m_bufferParams(bufferParams)
					{
					}

					~CreateBufferDeferred() = default;

					static void Execute(void* cmdData)
					{
						CreateBufferDeferred* cmdPtr = reinterpret_cast<CreateBufferDeferred*>(cmdData);

						cmdPtr->m_vertexStream->m_streamBuffer = re::Buffer::Create(
							cmdPtr->m_bufferName,
							cmdPtr->m_data->data().data(),
							util::CheckedCast<uint32_t>(cmdPtr->m_data->GetTotalNumBytes()),
							cmdPtr->m_bufferParams);
					}

					static void Destroy(void* cmdData)
					{
						CreateBufferDeferred* cmdPtr = reinterpret_cast<CreateBufferDeferred*>(cmdData);
						cmdPtr->~CreateBufferDeferred();
					}
				private:
					gr::VertexStream* m_vertexStream;
					std::string m_bufferName;
					std::unique_ptr<util::ByteVector> m_data;
					re::Buffer::BufferParams m_bufferParams;
				};
				
				re::RenderManager::Get()->EnqueueRenderCommand<CreateBufferDeferred>(
					newVertexStream.get(), bufferName, std::move(data), bufferParams);
			}
			else
			{
				// This will be moved back to the CTOR
				newVertexStream->m_streamBuffer = re::Buffer::Create(
					bufferName,
					data.data().data(),
					util::CheckedCast<uint32_t>(data.GetTotalNumBytes()),
					bufferParams);
			}
		}

		return newVertexStream;
	}


	std::shared_ptr<gr::VertexStream> VertexStream::Create(CreateParams&& createParams, bool queueBufferCreate /*= true*/)
	{
		return Create(createParams.m_streamDesc, std::move(*createParams.m_streamData.get()), queueBufferCreate);
	}


	VertexStream::VertexStream(StreamDesc const& createParams, util::ByteVector& data, bool& isNormalizedOut)
		: m_streamDesc(createParams)
	{
		SEAssert(m_streamDesc.m_type != Type::Type_Count && m_streamDesc.m_dataType != re::DataType::DataType_Count,
			"Invalid create params");

		SEAssert(m_streamDesc.m_type != Type::Index || 
			(m_streamDesc.m_dataType == re::DataType::UShort && data.IsScalarType<uint16_t>()) ||
			(m_streamDesc.m_dataType == re::DataType::UInt && data.IsScalarType<uint32_t>()),
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

			m_streamDesc.m_doNormalize = Normalize::False;
			isNormalized = true;
		}

		isNormalizedOut = isNormalized; 

		// Hash the incoming data before it is moved to the BufferAllocator:
		AddDataBytesToHash(data.data().data(), data.GetTotalNumBytes());

		// Hash any remaining properties
		ComputeDataHash();
	}


	void VertexStream::ComputeDataHash()
	{
		AddDataBytesToHash(m_streamDesc);
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
		return m_streamBuffer->GetTotalBytes() / DataTypeToStride(m_streamDesc.m_dataType);
	}


	re::DataType VertexStream::GetDataType() const
	{
		return m_streamDesc.m_dataType;
	}


	VertexStream::Normalize VertexStream::DoNormalize() const
	{
		return m_streamDesc.m_doNormalize;
	}


	re::VertexStreamView VertexStream::GetVertexStreamView() const
	{
		return re::VertexStreamView{
			.m_type = m_streamDesc.m_type,
			.m_dataType = m_streamDesc.m_dataType,
			.m_isNormalized = static_cast<bool>(m_streamDesc.m_doNormalize),
			.m_numElements = GetNumElements(),
		};
	}


	void VertexStream::ShowImGuiWindow() const
	{
		ImGui::Text(std::format("Type: {}", TypeToCStr(m_streamDesc.m_type)).c_str());

		const bool isMorphTarget = m_streamDesc.m_isMorphData == VertexStream::IsMorphData::True;
		ImGui::Text(std::format("Is morph target data? {}", isMorphTarget ? "true" : "false").c_str());

		ImGui::Text(std::format("Data type: {}", re::DataTypeToCStr(m_streamDesc.m_dataType)).c_str());
		ImGui::Text(std::format("Normalized? {}", (m_streamDesc.m_doNormalize ? "true" : "false")).c_str());

		ImGui::Text(std::format("Total data byte size: {}", GetTotalDataByteSize()).c_str());
		ImGui::Text(std::format("Number of elements: {}", GetNumElements()).c_str());
		ImGui::Text(std::format("Number of components: {}", DataTypeToNumComponents(m_streamDesc.m_dataType)).c_str());
		ImGui::Text(std::format("Component byte size: {}", DataTypeToComponentByteSize(m_streamDesc.m_dataType)).c_str());
	}
}