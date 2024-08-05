// © 2023 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Buffer.h"
#include "TransformRenderData.h"

#include "Core/Util/CastUtils.h"


namespace gr
{
	constexpr glm::vec3 Transform::WorldAxisX = glm::vec3(1.0f, 0.0f, 0.0f);
	constexpr glm::vec3 Transform::WorldAxisY = glm::vec3(0.0f, 1.0f, 0.0f);
	constexpr glm::vec3 Transform::WorldAxisZ = glm::vec3(0.0f, 0.0f, 1.0f); // Note: SaberEngine uses a RHCS


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


	std::shared_ptr<re::Buffer> Transform::CreateInstancedTransformBuffer(
		re::Buffer::Type bufferType, glm::mat4 const* model, glm::mat4* transposeInvModel)
	{
		InstancedTransformData const& transformData = 
			CreateInstancedTransformData(model, transposeInvModel);

		return re::Buffer::CreateArray(
			InstancedTransformData::s_shaderName,
			&transformData,
			1,
			bufferType);
	}


	std::shared_ptr<re::Buffer> Transform::CreateInstancedTransformBuffer(
		re::Buffer::Type bufferType, gr::Transform::RenderData const& transformData)
	{
		InstancedTransformData const& instancedMeshData = 
			CreateInstancedTransformData(transformData);

		return re::Buffer::CreateArray(
			InstancedTransformData::s_shaderName,
			&instancedMeshData,
			1,
			bufferType);
	}


	std::shared_ptr<re::Buffer> Transform::CreateInstancedTransformBuffer(
		re::Buffer::Type bufferType, std::vector<gr::Transform::RenderData const*> const& transformRenderData)
	{
		const uint32_t numInstances = util::CheckedCast<uint32_t>(transformRenderData.size());

		std::vector<InstancedTransformData> instancedMeshData;
		instancedMeshData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshData.emplace_back(CreateInstancedTransformData(*transformRenderData[transformIdx]));
		}

		std::shared_ptr<re::Buffer> instancedMeshBuffer = re::Buffer::CreateArray(
			InstancedTransformData::s_shaderName,
			&instancedMeshData[0],
			numInstances,
			bufferType);

		return instancedMeshBuffer;
	}
}