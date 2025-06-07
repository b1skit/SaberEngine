// © 2023 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "TransformRenderData.h"

#include "Renderer/Shaders/Common/TransformParams.h"


namespace
{
	TransformData CreateTransformData(gr::Transform::RenderData const& transformData)
	{
		return TransformData{
			.g_model = transformData.g_model,
			.g_transposeInvModel = transformData.g_transposeInvModel
		};
	}
}

namespace gr
{
	TransformData Transform::CreateTransformData(
		gr::Transform::RenderData const& transformData, IDType, gr::RenderDataManager const&)
	{
		return ::CreateTransformData(transformData);

	}


	TransformData Transform::CreateTransformData(
		glm::mat4 const* model, glm::mat4 const* transposeInvModel)
	{
		TransformData instancedMeshData{};

		instancedMeshData.g_model = model ? *model : glm::mat4(1.f);
		instancedMeshData.g_transposeInvModel = transposeInvModel ? *transposeInvModel : glm::mat4(1.f);

		return instancedMeshData;
	}


	std::shared_ptr<re::Buffer> Transform::CreateTransformBuffer(
		re::Lifetime lifetime,
		re::Buffer::StagingPool stagingPool,
		glm::mat4 const* model,
		glm::mat4 const* transposeInvModel)
	{
		TransformData const& transformData = CreateTransformData(model, transposeInvModel);

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


	re::BufferInput Transform::CreateTransformBufferInput(
		char const* shaderName,
		re::Lifetime lifetime,
		re::Buffer::StagingPool stagingPool,
		glm::mat4 const* model,
		glm::mat4 const* transposeInvModel)
	{
		return re::BufferInput(shaderName, CreateTransformBuffer(lifetime, stagingPool, model, transposeInvModel));
	}
}