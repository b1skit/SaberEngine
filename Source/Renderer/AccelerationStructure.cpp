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
							geometry.GetResourceHandle<re::VertexStreamResource_Position>(),
							geometry.GetResourceHandle<re::VertexStreamResource_Normal>(),
							geometry.GetResourceHandle<re::VertexStreamResource_Tangent>(),
							geometry.GetResourceHandle<re::VertexStreamResource_TexCoord>()
						),
						.g_UV1ColorIndex = glm::uvec4(
							k_invalidResourceHandle, // TODO: Support multiple UV sets
							geometry.GetResourceHandle<re::VertexStreamResource_Color>(),
							geometry.GetResourceHandle<re::VertexStreamResource_Index>(),
							geometry.GetResourceHandle<re::VertexStreamResource_Tangent>()
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