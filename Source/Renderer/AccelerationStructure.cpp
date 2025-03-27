// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "AccelerationStructure_Platform.h"
#include "Buffer.h"
#include "RenderManager.h"

#include "Core/Assert.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/CastUtils.h"

#include "Shaders/Common/BindlessResourceParams.h"


namespace
{
	uint32_t GetTotalGeometryCount(std::vector<std::shared_ptr<re::AccelerationStructure>> const& blasInstances)
	{
		uint32_t result = 0;
		for (auto const& instance : blasInstances)
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(instance->GetASParams());
			SEAssert(blasParams, "Failed to get BLASParams");

			result += util::CheckedCast<uint32_t>(blasParams->m_geometry.size());
		}
		return result;
	}
}

namespace re
{
	void AccelerationStructure::Geometry::RegisterResource(core::InvPtr<gr::VertexStream> const& vertexStream)
	{
		RegisterResourceInternal(
			re::IVertexStreamResource::GetResourceHandle(vertexStream),
			vertexStream->GetType(),
			vertexStream->GetDataType());
	}


	void AccelerationStructure::Geometry::RegisterResource(re::VertexBufferInput const& vertexBufferInput)
	{
		RegisterResourceInternal(
			re::IVertexStreamResource::GetResourceHandle(vertexBufferInput),
			vertexBufferInput.GetStream()->GetType(),
			vertexBufferInput.GetStream()->GetDataType());
	}


	void AccelerationStructure::Geometry::RegisterResourceInternal(
		ResourceHandle resolvedResourceHandle, gr::VertexStream::Type streamType, re::DataType dataType)
	{
		if (streamType == gr::VertexStream::Index)
		{
			switch (dataType)
			{
			case re::DataType::UShort:
			{
				m_indexStream16BitMetadata = VertexStreamMetadata{
					.m_resourceHandle = resolvedResourceHandle,
					.m_streamType = streamType,
					.m_setIndex = 0,
				};
			}
			break;
			case re::DataType::UInt:
			{
				m_indexStream32BitMetadata = VertexStreamMetadata{
					.m_resourceHandle = resolvedResourceHandle,
					.m_streamType = streamType,
					.m_setIndex = 1, // Typically only 1 index stream is allowed: setIdx = 1 here for consistency only
				};
			}
			break;
			default: SEAssertF("Unexpected index stream type");
			}
		}
		else
		{
			VertexStreamMetadata newStreamMetadata{
				.m_resourceHandle = resolvedResourceHandle,
				.m_streamType = streamType,
				.m_setIndex = 0,
			};

			for (size_t i = 0; i < m_vertexStreamMetadata.size(); ++i)
			{
				SEAssert(m_vertexStreamMetadata[i].m_streamType == gr::VertexStream::Type::Type_Count ||
					m_vertexStreamMetadata[i].m_streamType <= streamType,
					"Invalid insertion order. We currently assume streams will be added in the same order they're packed "
					"into MeshPrimitive::RenderData");

				if (static_cast<uint8_t>(m_vertexStreamMetadata[i].m_streamType) ==
					static_cast<uint8_t>(gr::VertexStream::Type::Type_Count) ||
					m_vertexStreamMetadata[i].m_streamType == streamType)
				{
					// If the current index has the same type as the new one, find first open spot:
					while (i + 1 < m_vertexStreamMetadata.size() &&
						m_vertexStreamMetadata[i].m_streamType == streamType)
					{
						++i;
						newStreamMetadata.m_setIndex++;
					}
					SEAssert(i < m_vertexStreamMetadata.size() &&
						m_vertexStreamMetadata[i].m_streamType == gr::VertexStream::Type::Type_Count,
						"Trying to add a new vertex stream with a set index > 0, but could not find a suitable location");

					// Insert into the empty element we found:
					m_vertexStreamMetadata[i] = newStreamMetadata;

					break;
				}
			}

			SEAssert(streamType != gr::VertexStream::Position ||
				newStreamMetadata.m_setIndex == 0,
				"Found multiple position streams. This is unexpected");
		}
	}


	ResourceHandle AccelerationStructure::Geometry::GetResourceHandle(
		gr::VertexStream::Type streamType, uint8_t setIdx /*= 0*/) const
	{
		if (streamType == gr::VertexStream::Type::Index)
		{
			switch (setIdx)
			{
			case 0:
			{
				return m_indexStream16BitMetadata.m_resourceHandle;
			}
			break;
			case 1:
			{
				return m_indexStream32BitMetadata.m_resourceHandle;
			}
			break;
			default: SEAssertF("Invalid setIdx. For gr::VertexStream::Type::Index, setIdx 0 = 16 bit, setIdx 1 = 32 bit");
			}
		}
		else
		{
			for (size_t i = 0; i < m_vertexStreamMetadata.size(); ++i)
			{
				// Searched all contiguously-packed elements and couldn't find a stream with the given type:
				if (m_vertexStreamMetadata[i].m_streamType == gr::VertexStream::Type::Type_Count)
				{
					return k_invalidResourceHandle;
				}

				if (m_vertexStreamMetadata[i].m_streamType == streamType)
				{
					SEAssert(i + setIdx < m_vertexStreamMetadata.size(), "Invalid set index");

					if (i + setIdx < m_vertexStreamMetadata.size() &&
						m_vertexStreamMetadata[i + setIdx].m_streamType == streamType)
					{
						return m_vertexStreamMetadata[i + setIdx].m_resourceHandle;
					}
					
					break;					
				}
			}
		}		

		// Searched all elements in a full array and couldn't find a stream with the given type:
		return k_invalidResourceHandle;
	}


	// ---


	std::shared_ptr<AccelerationStructure> AccelerationStructure::CreateBLAS(
		char const* name,
		std::unique_ptr<BLASParams>&& blasParams)
	{
		SEAssert(blasParams, "Invalid BLASParams");

		std::shared_ptr<AccelerationStructure> newAccelerationStructure;
		newAccelerationStructure.reset(
			new AccelerationStructure(
				name, 
				AccelerationStructure::Type::BLAS, 
				std::move(blasParams)));

		re::RenderManager::Get()->RegisterForCreate(newAccelerationStructure);

		return newAccelerationStructure;
	}


	std::shared_ptr<AccelerationStructure> AccelerationStructure::CreateTLAS(
		char const* name,
		std::unique_ptr<TLASParams>&& tlasParams)
	{
		SEAssert(tlasParams, "Invalid TLASParams");

		std::shared_ptr<AccelerationStructure> newAccelerationStructure;
		newAccelerationStructure.reset(
			new AccelerationStructure(
				name,
				AccelerationStructure::Type::TLAS,
				std::move(tlasParams)));

		re::RenderManager::Get()->RegisterForCreate(newAccelerationStructure);

		return newAccelerationStructure;
	}


	re::BufferInput AccelerationStructure::TLASParams::GetBindlessResourceLUT() const
	{
		return m_bindlessResourceLUT;
	}

	AccelerationStructure::~AccelerationStructure()
	{
		Destroy();
	}


	void AccelerationStructure::Create()
	{
		platform::AccelerationStructure::Create(*this);

		// Create the bindless LUT buffer:
		if (m_type == re::AccelerationStructure::Type::TLAS)
		{
			re::AccelerationStructure::TLASParams* tlasParams =
				dynamic_cast<re::AccelerationStructure::TLASParams*>(m_asParams.get());
			SEAssert(tlasParams, "Failed to get TLASParams");

			SEAssert(tlasParams->m_bindlessResourceLUT.GetBuffer() == nullptr,
				"Bindless resource LUT buffer already created. This is unexpected");

			std::vector<BindlessLUTData> bindlessLUTData;
			bindlessLUTData.reserve(GetTotalGeometryCount(tlasParams->m_blasInstances));

			for (auto const& instance : tlasParams->m_blasInstances)
			{
				re::AccelerationStructure::BLASParams const* blasParams =
					dynamic_cast<re::AccelerationStructure::BLASParams const*>(instance->GetASParams());
				SEAssert(blasParams, "Failed to get BLASParams");

				for (auto const& geometry : blasParams->m_geometry)
				{
					bindlessLUTData.emplace_back(BindlessLUTData{
						.g_posNmlTanUV0 = glm::uvec4(
							geometry.GetResourceHandle(gr::VertexStream::Position),
							geometry.GetResourceHandle(gr::VertexStream::Normal),
							geometry.GetResourceHandle(gr::VertexStream::Tangent),
							geometry.GetResourceHandle(gr::VertexStream::TexCoord, 0)
						),
						.g_UV1ColorIndex = glm::uvec4(
							geometry.GetResourceHandle(gr::VertexStream::TexCoord, 1),
							geometry.GetResourceHandle(gr::VertexStream::Color),
							geometry.GetResourceHandle(gr::VertexStream::Index, 0), // 16 bit
							geometry.GetResourceHandle(gr::VertexStream::Index, 1) // 32 bit
						),
					});
				}
			}
			SEStaticAssert(sizeof(BindlessLUTData) == 32, "BindlessLUTData size has changed: This must be updated");

			tlasParams->m_bindlessResourceLUT = re::BufferInput(
				BindlessLUTData::s_shaderName,
				re::Buffer::CreateArray(
					"TLAS Bindless LUT",
					bindlessLUTData.data(),
					re::Buffer::BufferParams{
						.m_lifetime = re::Lifetime::Permanent,
						.m_stagingPool = re::Buffer::StagingPool::Temporary,
						.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
						.m_accessMask = re::Buffer::Access::GPURead,
						.m_usageMask = re::Buffer::Usage::Constant,
						.m_arraySize = util::CheckedCast<uint32_t>(bindlessLUTData.size()),
					}));
		}
	}


	void AccelerationStructure::Destroy()
	{
		if (m_platformParams)
		{
			re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platformParams));
		}
	}


	AccelerationStructure::AccelerationStructure(char const* name, Type type, std::unique_ptr<IASParams>&& createParams)
		: core::INamedObject(name)
		, m_platformParams(platform::AccelerationStructure::CreatePlatformParams())
		, m_asParams(std::move(createParams))
		, m_type(type)
	{
	}
}