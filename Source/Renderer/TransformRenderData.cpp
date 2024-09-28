// © 2023 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Buffer.h"
#include "TransformRenderData.h"

#include "Core/Util/CastUtils.h"


namespace gr
{
	InstancedTransformData Transform::CreateInstancedTransformData(
		gr::Transform::RenderData const& transformData)
	{
		return InstancedTransformData {
			.g_model = transformData.g_model,
			.g_transposeInvModel = transformData.g_transposeInvModel
		};
	}


	InstancedTransformData Transform::CreateInstancedTransformData(
		glm::mat4 const* model, glm::mat4 const* transposeInvModel)
	{
		InstancedTransformData instancedMeshData{};

		instancedMeshData.g_model = model ? *model : glm::mat4(1.f);
		instancedMeshData.g_transposeInvModel = transposeInvModel ? *transposeInvModel : glm::mat4(1.f);

		return instancedMeshData;
	}


	re::BufferInput Transform::CreateInstancedTransformBuffer(
		re::Buffer::AllocationType bufferAlloc, glm::mat4 const* model, glm::mat4* transposeInvModel)
	{
		InstancedTransformData const& transformData = 
			CreateInstancedTransformData(model, transposeInvModel);

		return re::BufferInput(
			InstancedTransformData::s_shaderName,
			re::Buffer::CreateArray(
				InstancedTransformData::s_shaderName,
				&transformData,
				re::Buffer::BufferParams{
					.m_allocationType = bufferAlloc,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
					.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
					.m_type = re::Buffer::Type::Structured,
					.m_arraySize = 1,
				}));
	}


	re::BufferInput Transform::CreateInstancedTransformBuffer(
		re::Buffer::AllocationType bufferAlloc, gr::Transform::RenderData const& transformData)
	{
		InstancedTransformData const& instancedMeshData = 
			CreateInstancedTransformData(transformData);

		return re::BufferInput(
			InstancedTransformData::s_shaderName,
			re::Buffer::CreateArray(
				InstancedTransformData::s_shaderName,
				&instancedMeshData,
				re::Buffer::BufferParams{
					.m_allocationType = bufferAlloc,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
					.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
					.m_type = re::Buffer::Type::Structured,
					.m_arraySize = 1,
				}));
	}


	re::BufferInput Transform::CreateInstancedTransformBuffer(
		re::Buffer::AllocationType bufferAlloc, std::vector<gr::Transform::RenderData const*> const& transformRenderData)
	{
		const uint32_t numInstances = util::CheckedCast<uint32_t>(transformRenderData.size());

		std::vector<InstancedTransformData> instancedMeshData;
		instancedMeshData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshData.emplace_back(CreateInstancedTransformData(*transformRenderData[transformIdx]));
		}

		return re::BufferInput(
			InstancedTransformData::s_shaderName,
			re::Buffer::CreateArray(
				InstancedTransformData::s_shaderName,
				&instancedMeshData[0],
				re::Buffer::BufferParams{
					.m_allocationType = bufferAlloc,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
					.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
					.m_type = re::Buffer::Type::Structured,
					.m_arraySize = numInstances,
				}));
	}
}