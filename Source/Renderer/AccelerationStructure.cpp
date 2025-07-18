// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "AccelerationStructure_Platform.h"
#include "BindlessResource.h"
#include "Buffer.h"
#include "BufferView.h"
#include "Context.h"
#include "Effect.h"

#include "Core/Assert.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/CastUtils.h"

#include "Renderer/Shaders/Common/RayTracingParams.h"


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
		std::vector<uint32_t>& blasGeoOwnerIDsOut)
	{
		const uint32_t geoCount = GetTotalGeometryCount(blasInstances);

		blasGeoOwnerIDsOut.clear();
		blasGeoOwnerIDsOut.reserve(geoCount);

		std::vector<VertexStreamLUTData> vertexStreamLUTData;
		vertexStreamLUTData.reserve(geoCount);

		for (auto const& instance : blasInstances)
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(instance->GetASParams());
			SEAssert(blasParams, "Failed to get BLASParams");

			for (auto const& geometry : blasParams->m_geometry)
			{
				vertexStreamLUTData.emplace_back(VertexStreamLUTData{
					.g_posNmlTanUV0Index = glm::uvec4(
						geometry.GetResourceHandle(re::VertexStream::Position),
						geometry.GetResourceHandle(re::VertexStream::Normal),
						geometry.GetResourceHandle(re::VertexStream::Tangent),
						geometry.GetResourceHandle(re::VertexStream::TexCoord, 0)
					),
					.g_UV1ColorIndex = glm::uvec4(
						geometry.GetResourceHandle(re::VertexStream::TexCoord, 1),
						geometry.GetResourceHandle(re::VertexStream::Color),
						geometry.GetResourceHandle(re::VertexStream::Index, 0), // 16 bit
						geometry.GetResourceHandle(re::VertexStream::Index, 1) // 32 bit
					),
				});

				blasGeoOwnerIDsOut.emplace_back(geometry.GetOwnerID());
			}
		}
		SEStaticAssert(sizeof(VertexStreamLUTData) == 32, "VertexStreamLUTData size has changed: This must be updated");

		SEAssert(blasGeoOwnerIDsOut.size() == geoCount && vertexStreamLUTData.size() == geoCount,
			"Unexpected size mismatch");

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
		core::InvPtr<re::VertexStream> const& vertexStream, bool forceReplace /*= false*/)
	{
		RegisterResourceInternal(
			vertexStream->GetResourceHandle(),
			vertexStream->GetType(),
			vertexStream->GetDataType(),
			forceReplace);
	}


	void AccelerationStructure::Geometry::RegisterResource(
		re::VertexBufferInput const& vertexBufferInput, bool forceReplace /*= false*/)
	{
		RegisterResourceInternal(
			vertexBufferInput.GetStream()->GetResourceHandle(),
			vertexBufferInput.GetStream()->GetType(),
			vertexBufferInput.GetStream()->GetDataType(),
			forceReplace);
	}


	void AccelerationStructure::Geometry::RegisterResourceInternal(
		ResourceHandle resolvedResourceHandle,
		re::VertexStream::Type streamType,
		re::DataType dataType,
		bool forceReplace /*= false*/)
	{
		if (streamType == re::VertexStream::Index)
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
				SEAssert(m_vertexStreamMetadata[i].m_streamType == re::VertexStream::Type::Type_Count ||
					m_vertexStreamMetadata[i].m_streamType <= streamType,
					"Invalid insertion order. We currently assume streams will be added in the same order they're packed "
					"into MeshPrimitive::RenderData");

				if (m_vertexStreamMetadata[i].m_streamType == re::VertexStream::Type::Type_Count ||
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
							m_vertexStreamMetadata[i].m_streamType == re::VertexStream::Type::Type_Count,
							"Trying to add a new vertex stream with a set index > 0, but could not find a suitable location");
					}

					// Insert into the empty element we found:
					m_vertexStreamMetadata[i] = newStreamMetadata;

					break;
				}
			}

			SEAssert(streamType != re::VertexStream::Position ||
				newStreamMetadata.m_setIndex == 0,
				"Found multiple position streams. This is unexpected");
		}
	}


	ResourceHandle AccelerationStructure::Geometry::GetResourceHandle(
		re::VertexStream::Type streamType, uint8_t setIdx /*= 0*/) const
	{
		if (streamType == re::VertexStream::Type::Index)
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
			default: SEAssertF("Invalid setIdx. For re::VertexStream::Type::Index, setIdx 0 = 16 bit, setIdx 1 = 32 bit");
			}
		}
		else
		{
			for (size_t i = 0; i < m_vertexStreamMetadata.size(); ++i)
			{
				// Searched all contiguously-packed elements and couldn't find a stream with the given type:
				if (m_vertexStreamMetadata[i].m_streamType == re::VertexStream::Type::Type_Count)
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

		newAccelerationStructure->m_platObj->GetContext()->RegisterForCreate(newAccelerationStructure);

		return newAccelerationStructure;
	}


	std::shared_ptr<AccelerationStructure> AccelerationStructure::CreateTLAS(
		char const* name,
		std::unique_ptr<TLASParams>&& tlasParams,
		std::map<EffectID, re::ShaderBindingTable::SBTParams>&& sbtParams)
	{
		SEAssert(tlasParams, "Invalid TLASParams");

		std::shared_ptr<AccelerationStructure> newAccelerationStructure;
		newAccelerationStructure.reset(new AccelerationStructure(
			name, AccelerationStructure::Type::TLAS, std::move(tlasParams)));

		// Get a bindless resource handle:
		re::BindlessResourceManager* brm = newAccelerationStructure->m_platObj->GetContext()->GetBindlessResourceManager();
		SEAssert(brm, "Failed to get BindlessResourceManager");
		
		re::AccelerationStructure::TLASParams* newTLASParams =
			dynamic_cast<re::AccelerationStructure::TLASParams*>(newAccelerationStructure->m_asParams.get());

		newTLASParams->m_srvTLASResourceHandle = brm->RegisterResource(
			std::make_unique<re::AccelerationStructureResource>(newAccelerationStructure));

		// Create the bindless LUT buffer:
		newTLASParams->m_bindlessResourceLUT =
			CreateBindlessLUT(newTLASParams->m_blasInstances, newTLASParams->m_blasGeoOwnerIDs);

		// Create the ShaderBindingTable:
		for (auto const& entry : sbtParams)
		{
			
			newTLASParams->m_SBTs.emplace(
				entry.first,
				re::ShaderBindingTable::Create(
					entry.first.GetEffect()->GetName().c_str(), entry.second, newAccelerationStructure));
		}
		
		// Register for API creation:
		newAccelerationStructure->m_platObj->GetContext()->RegisterForCreate(newAccelerationStructure);

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
			m_platObj->GetContext()->RegisterForDeferredDelete(std::move(m_platObj));
		}

		if (m_type == re::AccelerationStructure::Type::TLAS &&
			GetResourceHandle() != INVALID_RESOURCE_IDX)
		{
			re::BindlessResourceManager* brm = m_platObj->GetContext()->GetBindlessResourceManager();
			SEAssert(brm, "Failed to get BindlessResourceManager. This should not be possible");

			re::AccelerationStructure::TLASParams* tlasParams =
				dynamic_cast<re::AccelerationStructure::TLASParams*>(m_asParams.get());
			SEAssert(tlasParams, "Failed to cast to TLASParams");

			brm->UnregisterResource(tlasParams->m_srvTLASResourceHandle);

			for (auto& sbt : tlasParams->m_SBTs)
			{
				sbt.second->Destroy();
			}
			tlasParams->m_SBTs.clear();
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