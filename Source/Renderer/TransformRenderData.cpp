// © 2023 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "TransformRenderData.h"

#include "Core/Util/CastUtils.h"


namespace gr
{
	TransformData Transform::CreateInstancedTransformData(
		gr::Transform::RenderData const& transformData)
	{
		return TransformData {
			.g_model = transformData.g_model,
			.g_transposeInvModel = transformData.g_transposeInvModel
		};
	}


	TransformData Transform::CreateInstancedTransformData(
		glm::mat4 const* model, glm::mat4 const* transposeInvModel)
	{
		TransformData instancedMeshData{};

		instancedMeshData.g_model = model ? *model : glm::mat4(1.f);
		instancedMeshData.g_transposeInvModel = transposeInvModel ? *transposeInvModel : glm::mat4(1.f);

		return instancedMeshData;
	}


	std::shared_ptr<re::Buffer> Transform::CreateInstancedTransformBuffer(
		re::Lifetime lifetime,
		re::Buffer::StagingPool stagingPool,
		glm::mat4 const* model,
		glm::mat4 const* transposeInvModel)
	{
		TransformData const& transformData =
			CreateInstancedTransformData(model, transposeInvModel);

		return re::Buffer::CreateArray(
				"InstancedTransformBufferArrayFromPtrs",
				&transformData,
				re::Buffer::BufferParams{
					.m_lifetime = lifetime,
					.m_stagingPool = stagingPool,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Structured,
					.m_arraySize = 1,
				});
	}


	std::shared_ptr<re::Buffer> Transform::CreateInstancedTransformBuffer(
		re::Lifetime lifetime,
		re::Buffer::StagingPool stagingPool,
		gr::Transform::RenderData const& transformData)
	{
		TransformData const& instancedMeshData =
			CreateInstancedTransformData(transformData);

		return re::Buffer::CreateArray(
			"InstancedTransformBufferArrayFromRenderData",
			&instancedMeshData,
			re::Buffer::BufferParams{
				.m_lifetime = lifetime,
				.m_stagingPool = stagingPool,
				.m_memPoolPreference = re::Buffer::UploadHeap,
				.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
				.m_usageMask = re::Buffer::Structured,
				.m_arraySize = 1,
			});
	}


	std::shared_ptr<re::Buffer> Transform::CreateInstancedTransformBuffer(
		re::Lifetime lifetime,
		re::Buffer::StagingPool stagingPool, 
		std::vector<gr::Transform::RenderData const*> const& transformRenderDatas)
	{
		const uint32_t numInstances = util::CheckedCast<uint32_t>(transformRenderDatas.size());

		std::vector<TransformData> instancedMeshData;
		instancedMeshData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshData.emplace_back(CreateInstancedTransformData(*transformRenderDatas[transformIdx]));
		}

		return re::Buffer::CreateArray(
			"InstancedTransformBufferArrayFromRenderDatas",
			&instancedMeshData[0],
			re::Buffer::BufferParams{
				.m_lifetime = lifetime,
				.m_stagingPool = stagingPool,
				.m_memPoolPreference = re::Buffer::UploadHeap,
				.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
				.m_usageMask = re::Buffer::Structured,
				.m_arraySize = numInstances,
			});
	}


	re::BufferInput Transform::CreateInstancedTransformBufferInput(
		char const* shaderName,
		re::Lifetime lifetime,
		re::Buffer::StagingPool stagingPool,
		glm::mat4 const* model,
		glm::mat4 const* transposeInvModel)
	{
		return re::BufferInput(shaderName, CreateInstancedTransformBuffer(lifetime, stagingPool, model, transposeInvModel));
	}


	re::BufferInput Transform::CreateInstancedTransformBufferInput(
		char const* shaderName,
		re::Lifetime lifetime,
		re::Buffer::StagingPool stagingPool,
		gr::Transform::RenderData const& transformData)
	{
		return re::BufferInput(shaderName, CreateInstancedTransformBuffer(lifetime, stagingPool, transformData));
	}


	re::BufferInput Transform::CreateInstancedTransformBufferInput(
		char const* shaderName,
		re::Lifetime bufLifetime,
		re::Buffer::StagingPool stagingPool,
		std::vector<gr::Transform::RenderData const*> const& transformRenderData)
	{
		return re::BufferInput(shaderName, CreateInstancedTransformBuffer(bufLifetime, stagingPool, transformRenderData));
	}
}