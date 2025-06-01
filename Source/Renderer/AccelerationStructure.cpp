// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "AccelerationStructure_Platform.h"
#include "BindlessResource.h"
#include "Buffer.h"
#include "BufferView.h"
#include "RenderManager.h"

#include "Core/Assert.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/CastUtils.h"

#include "Shaders/Common/RayTracingParams.h"



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


	re::BufferInput CreateBindlessLUT(std::vector<std::shared_ptr<re::AccelerationStructure>> const& blasInstances,
		std::vector<gr::RenderDataID>& blasGeoRenderDataIDsOut)
	{
		blasGeoRenderDataIDsOut.clear();

		std::vector<VertexStreamLUTData> vertexStreamLUTData;
		vertexStreamLUTData.reserve(GetTotalGeometryCount(blasInstances));

		for (auto const& instance : blasInstances)
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(instance->GetASParams());
			SEAssert(blasParams, "Failed to get BLASParams");

			for (auto const& geometry : blasParams->m_geometry)
			{
				vertexStreamLUTData.emplace_back(VertexStreamLUTData{
					.g_posNmlTanUV0Index = glm::uvec4(
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

				blasGeoRenderDataIDsOut.emplace_back(geometry.GetRenderDataID());
			}
		}
		SEStaticAssert(sizeof(VertexStreamLUTData) == 32, "VertexStreamLUTData size has changed: This must be updated");

		return re::BufferInput(
			VertexStreamLUTData::s_shaderName,
			re::Buffer::CreateArray(
				"TLAS Bindless LUT",
				vertexStreamLUTData.data(),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
					.m_accessMask = re::Buffer::Access::GPURead,
					.m_usageMask = re::Buffer::Usage::Structured,
					.m_arraySize = util::CheckedCast<uint32_t>(vertexStreamLUTData.size()),
			}));
	}
}

namespace re
{
	void AccelerationStructure::Geometry::RegisterResource(
		core::InvPtr<gr::VertexStream> const& vertexStream, bool forceReplace /*= false*/)
	{
		RegisterResourceInternal(
			vertexStream->GetBindlessResourceHandle(),
			vertexStream->GetType(),
			vertexStream->GetDataType(),
			forceReplace);
	}


	void AccelerationStructure::Geometry::RegisterResource(
		re::VertexBufferInput const& vertexBufferInput, bool forceReplace /*= false*/)
	{
		RegisterResourceInternal(
			vertexBufferInput.GetStream()->GetBindlessResourceHandle(),
			vertexBufferInput.GetStream()->GetType(),
			vertexBufferInput.GetStream()->GetDataType(),
			forceReplace);
	}


	void AccelerationStructure::Geometry::RegisterResourceInternal(
		ResourceHandle resolvedResourceHandle,
		gr::VertexStream::Type streamType,
		re::DataType dataType,
		bool forceReplace /*= false*/)
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

				if (m_vertexStreamMetadata[i].m_streamType == gr::VertexStream::Type::Type_Count ||
					m_vertexStreamMetadata[i].m_streamType == streamType)
				{
					// If the current index has the same type as the new one, find first open spot:
					if (forceReplace == false)
					{
						while (i + 1 < m_vertexStreamMetadata.size() &&
							m_vertexStreamMetadata[i].m_streamType == streamType)
						{
							++i;
							newStreamMetadata.m_setIndex++;
						}
						SEAssert(i < m_vertexStreamMetadata.size() &&
							m_vertexStreamMetadata[i].m_streamType == gr::VertexStream::Type::Type_Count,
							"Trying to add a new vertex stream with a set index > 0, but could not find a suitable location");
					}

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
					return INVALID_RESOURCE_IDX;
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
		return INVALID_RESOURCE_IDX;
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
		newAccelerationStructure.reset(new AccelerationStructure(
			name, AccelerationStructure::Type::TLAS, std::move(tlasParams)));

		// Get a bindless resource handle:
		re::BindlessResourceManager* brm = re::Context::Get()->GetBindlessResourceManager();
		SEAssert(brm, "Failed to get BindlessResourceManager");
		
		re::AccelerationStructure::TLASParams* movedTlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams*>(newAccelerationStructure->m_asParams.get());

		movedTlasParams->m_srvTLASResourceHandle = brm->RegisterResource(
			std::make_unique<re::AccelerationStructureResource>(newAccelerationStructure));

		// Create the bindless LUT buffer:
		movedTlasParams->m_bindlessResourceLUT =
			CreateBindlessLUT(movedTlasParams->m_blasInstances, movedTlasParams->m_blasGeoRenderDataIDs);

		// Register for API creation:
		re::RenderManager::Get()->RegisterForCreate(newAccelerationStructure);

		return newAccelerationStructure;
	}


	ResourceHandle AccelerationStructure::TLASParams::GetResourceHandle() const
	{
		return m_srvTLASResourceHandle;
	}


	re::BufferInput const& AccelerationStructure::TLASParams::GetBindlessVertexStreamLUT() const
	{
		return m_bindlessResourceLUT;
	}


	AccelerationStructure::~AccelerationStructure()
	{
		Destroy();
	}


	void AccelerationStructure::Destroy()
	{
		if (m_platObj)
		{
			re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platObj));
		}

		if (m_type == re::AccelerationStructure::Type::TLAS &&
			GetResourceHandle() != INVALID_RESOURCE_IDX)
		{
			re::BindlessResourceManager* brm = re::Context::Get()->GetBindlessResourceManager();
			SEAssert(brm, "Failed to get BindlessResourceManager. This should not be possible");

			re::AccelerationStructure::TLASParams* tlasParams =
				dynamic_cast<re::AccelerationStructure::TLASParams*>(m_asParams.get());
			SEAssert(tlasParams, "Failed to cast to TLASParams");

			brm->UnregisterResource(
				tlasParams->m_srvTLASResourceHandle,
				re::RenderManager::Get()->GetCurrentRenderFrameNum());
		}
	}


	AccelerationStructure::AccelerationStructure(char const* name, Type type, std::unique_ptr<IASParams>&& createParams)
		: core::INamedObject(name)
		, m_platObj(platform::AccelerationStructure::CreatePlatformObject())
		, m_asParams(std::move(createParams))
		, m_type(type)
	{
	}
}